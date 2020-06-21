// wonclient.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"

#include "win32_critsec.h"

#include "MINTCLIENT.h"
#include "Directory.h"
#include "Identity.h"
#include "RoutingServerClient.h"


namespace MINTCLIENT
{
    // Bookkeeping
    static const U32 maxClients = 1;
    static Win32::CritSec clientsCritSec;
    static Client* clients[maxClients];

    // Constructor
    Client::Client(const Config& config)
        : config(config)
        , flags(0)
        , socket(Win32::Socket::TCP, TRUE)
        , packetBuffer(StyxNet::Packet::Buffer::Create(StyxNet::clientBufferSize))
    {
        StyxNet::AddClient();

        clientsCritSec.Enter();

        index = U32(-1);
        for (U32 c = 0; c < maxClients; c++)
        {
            if (!clients[c])
            {
                index = c;
                clients[index] = this;
            }
        }
        clientsCritSec.Exit();

        // Start the thread
        thread.Start(ThreadProc, this);

        // Make it above normal
        thread.SetPriority(Win32::Thread::ABOVE_NORMAL);
    }

    // Destructor
    Client::~Client()
    {
        clientsCritSec.Enter();
        clients[index] = nullptr;
        clientsCritSec.Exit();

        // If we're connected, disconnect us
        if (flags & StyxNet::ClientFlags::Connected)
        {
            StyxNet::Packet::Create(MINTCLIENT::Message::RoutingServerDisconnect).Send(socket);
        }

        // Make sure the socket is closed
        socket.Close();

        delete packetBuffer;

        StyxNet::RemoveClient();
    }

    void Client::SendCommand(CommandContext* context)
    {
        LDIAG("MINTCLIENT::Client::SendCommand");

        commandContext = context;
        eventCommand.Signal();
    }

    //
    // Client::ThreadProc
    //
    // Thread procedure
    //
    U32 STDCALL Client::ThreadProc(void* context)
    {
        LOG_DIAG(("MINTCLIENT main thread enter."));

        Client* client = static_cast<Client*>(context);

        // Initiate the connection
        client->socket.Bind(Win32::Socket::Address(ADDR_ANY, 0));
        client->socket.EventSelect(client->event, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);
        if (!client->socket.Connect(client->config.address))
        {
            LWARN("Could not connect socket to " << client->config.address);
            client->SendEvent(StyxNet::EventMessage::ServerConnectFailed);
        }

        Bool quit = FALSE;

        Win32::EventIndex::List<3> events;
        events.AddEvent(client->event, NULL);
        events.AddEvent(client->eventQuit, client);
        events.AddEvent(client->eventCommand, &client->commandContext);

        U32 nextTime = Clock::Time::Ms();

        while (!quit)
        {
            void* context;

            S32 remaining = nextTime - Clock::Time::Ms();

            if (remaining > 0 && events.Wait(context, FALSE, remaining))
            {
                if (!context)
                {
                    Win32::Socket::NetworkEvents networkEvents;
                    client->socket.EnumEvents(client->event, networkEvents);

                    if (networkEvents.GetEvents() & FD_CONNECT)
                    {
                        S32 error = networkEvents.GetError(FD_CONNECT_BIT);
                        if (error)
                        {
                            LDIAG("Connection to server failed");
                        }
                        else
                        {
                            LDIAG("Connection established with server");
                            client->flags |= StyxNet::ClientFlags::Connected;
                        }
                    }

                    if (networkEvents.GetEvents() & FD_READ)
                    {
                        // Get the packet system to accept the data from the socket
                        StyxNet::Packet::Accept(*client->packetBuffer, client->socket);

                        // Extract the packets out of the buffer
                        while (const StyxNet::Packet* pkt = StyxNet::Packet::Extract(*client->packetBuffer))
                        {
                            // Have the client process the packet
                            client->ProcessPacket(*pkt);
                        }
                    }
                }
                else
                {
                    // A command queued with the event system.
                    if (context == &client->commandContext)
                    {
                        // If we're connected, send off the command else requeue.
                        if ((client->flags & StyxNet::ClientFlags::Connected) == StyxNet::ClientFlags::Connected)
                        {
                            if (client->GetCommandContext()->wasProcessed)
                            {
                                LDIAG("A command which was already processed has been requeued. Is this correct?");
                            }

                            // Send off the command, handle in network event callback (ProcessPacket).
                            client->GetCommandContext()->Send();
                        }
                        else
                        {
                            // Not connected yet, requeue the command.
                            client->SendCommand(client->GetCommandContext());
                            Sleep(1000);
                        }
                    }
                    else if (context == &client)
                    {
                        quit = true;
                    }
                }
            }
            else
            {
                nextTime += 500;
            }
        }

        // Delete the client
        client->thread.Clear();
        delete client;

        LOG_DIAG(("MINTCLIENT main thread exit."));

        return (0x6666);
    }

    //
    // Handle incoming packet, route to appropriate handler.
    //
    void MINTCLIENT::Client::ProcessPacket(const StyxNet::Packet& packet)
    {
        switch (packet.GetCommand())
        {
            case MINTCLIENT::Message::ServerConnect:
            {
                LDIAG("Successfully connected to server");
                flags |= StyxNet::ClientFlags::Connected;
            }
            break;

            // For a known packet, if it's the response to a command, pull up command context and deal with it.
            case MINTCLIENT::Message::DirectoryListServers:
                commandContext->commandDone.Signal();
                break;

            case MINTCLIENT::Message::DirectoryListRooms:
                commandContext->commandDone.Signal();
                break;

            case MINTCLIENT::Message::IdentityAuthenticate:
            {
                auto r = MINTCLIENT::Identity::Result();
                r.error = WONAPI::Error_Success;
                r.message.Set("MINTCLIENT.cpp::229");
                commandContext->SetData(r);
                commandContext->commandDone.Signal();
            }
            break;

            case MINTCLIENT::Message::RoutingServerRoomConnect:
            case MINTCLIENT::Message::RoutingServerRoomRegister:
            case MINTCLIENT::Message::RoutingServerGetUserList:
                commandContext->commandDone.Signal();
                break;

            case MINTCLIENT::Message::RoutingServerBroadcastChat:
                commandContext->SetData(*(RoutingServerClient::ASCIIChatMessageResult*)packet.GetData());
                commandContext->commandDone.Signal();
                break;

            case MINTCLIENT::Message::RoutingServerDisconnect:
            case MINTCLIENT::Message::ServerShutdown:
                eventQuit.Signal();
                break;

            default:
                // Unknown packet command
                LDIAG("Unknown Packet Command " << HEX(packet.GetCommand(), 8) << " from server");
                break;
        }
    }
}

void WONInitialize()
{
    // Get this library ready for use.
}

void WONTerminate()
{
    // Shutdown this library.
}
