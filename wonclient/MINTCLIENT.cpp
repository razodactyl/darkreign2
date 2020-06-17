// wonclient.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"

#include "MINTCLIENT.h"
#include "Identity.h"

#include "styxnet_clientmessage.h"
#include "styxnet_serverresponse.h"
#include "win32_critsec.h"


namespace MINTCLIENT
{
    Win32::Thread t;

    template<class T>
    U32 STDCALL Process(void* context)
    {
        auto cl = static_cast<Client::ContextList<T>*>(context);

        bool doneProcessing = false;

        while (!doneProcessing)
        {
            void* ctx;

            if (cl->events.Wait(ctx, TRUE, 1000))
            {
                const auto cc = static_cast<Client::CommandContext*>(ctx);
                switch (cc->command)
                {
                case 0x11223344:
                    DirectoryResult result;
                    auto directoryEntity = DirectoryEntity();
                    directoryEntity.mName = L"AuthServer";
                    result.entityList.push_back(directoryEntity);
                    result.error = WONAPI::Error_Success;
                    cl->callback(result);
                    break;
                }

                doneProcessing = true;
            }
        }

        return TRUE;
    }

    //
    // MINT:Router:GetDirectoryEx
    //
    WONAPI::Error GetDirectoryEx(
        MINTCLIENT::Identity* identity,
        const std::vector<MINTCLIENT::IPSocket::Address>* servers,
        void (*callback)(const MINTCLIENT::DirectoryResult& result)
    )
    {
        auto* cl = new MINTCLIENT::Client::ContextList<MINTCLIENT::DirectoryResult>();
        cl->callback = callback;

        for (auto server = servers->begin(); server != servers->end(); ++server)
        {
            MINTCLIENT::Client::Config* c = new MINTCLIENT::Client::Config(Win32::Socket::Address(server->ip, U16(server->port)));
            MINTCLIENT::Client* client = new MINTCLIENT::Client(*c);

            // Go away, process the command, callback if there's a result.
            auto ctx = new Client::CommandContext(client);
            ctx->command = 0x11223344;

            cl->Add(ctx);

            client->SendCommand(ctx);
        }

        // Should have finished by now.
        ASSERT(!t.IsAlive());

        t.Start(Process<MINTCLIENT::DirectoryResult>, cl);

        return WONAPI::Error_Pending;
    }

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
        clients[index] = NULL;
        clientsCritSec.Exit();

        // If we're connected, disconnect us
        if (flags & StyxNet::ClientFlags::Connected)
        {
            StyxNet::Packet::Create(StyxNet::ClientMessage::UserLogout).Send(socket);
        }

        // Make sure the socket is closed
        socket.Close();

        // // If there's a session, delete it
        // if (session)
        // {
        //     delete session;
        // }
        //

        delete packetBuffer;

        StyxNet::RemoveClient();
    }

    void Client::SendCommand(CommandContext* context)
    {
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
        Client* client = static_cast<Client*>(context);

        LOG_DIAG(("Proc"));

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
                            StyxNet::Packet& pkt = StyxNet::Packet::Create(
                                client->GetCommandContext()->command,
                                0
                            );
                            // Send off the command, handle in network event callback (ProcessPacket).
                            pkt.Send(client->socket);
                        }
                        else
                        {
                            // Not connected yet, requeue the command.
                            client->SendCommand(client->GetCommandContext());
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

        return (0x6666);
    }

    //
    // Handle incoming packet, route to approprite handler.
    //
    void MINTCLIENT::Client::ProcessPacket(const StyxNet::Packet& packet)
    {
        switch (packet.GetCommand())
        {
        case StyxNet::ServerResponse::UserConnected:
            LDIAG("Successfully connected to server");
            flags |= StyxNet::ClientFlags::Connected;
            break;

        case 0x11223344:
            commandContext->commandDone.Signal();
            break;

        case MINTCLIENT::Identity::AuthenticateCommand:
            commandContext->commandDone.Signal();
            break;

        case 0x44332211: // Server is shutting down.
            eventQuit.Signal();
            break;

        default:
            // Unknown packet command
            LDIAG("Unknown Packet Command " << HEX(packet.GetCommand(), 8) << " from server");
            break;

            // For a known packet, if it's the response to a command, pull up command context and deal with it.
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
