// wonclient.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"

#include "win32_critsec.h"

#include "MINTCLIENT.h"
#include "Directory.h"
#include "Encoding.h"
#include "Identity.h"
#include "RoutingServerClient.h"
#include "win32_dns.h"


namespace MINTCLIENT
{
    // Bookkeeping
    static const U32 maxClients = 1;
    static Win32::CritSec clientsCritSec;
    static Client* clients[maxClients];
    static Win32::CritSec commandsCrit;

    void DNSCallback(const Win32::DNS::Host* host, void* context)
    {
        Client* client = static_cast<Client*>(context);

        if (host) {
            client->config.address = *host->GetAddress();
            client->config.address.SetPort(client->config._address.GetPortI());

            // Start the thread
            client->GetThread()->Start(Client::MINTMasterThread, client);

            // Make it above normal
            client->GetThread()->SetPriority(Win32::Thread::ABOVE_NORMAL);
        }
    }

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
                break;
            }
        }
        clientsCritSec.Exit();

        Win32::DNS::Host* host;
        Win32::DNS::GetByName(config.host, host, DNSCallback, this);
    }

    // Destructor
    Client::~Client()
    {
        clientsCritSec.Enter();
        clients[index] = nullptr;
        clientsCritSec.Exit();

        // If connected, tell the server on the way out.
        if (flags & StyxNet::ClientFlags::Connected)
        {
            // Just send it, don't care about response.
            StyxNet::Packet::Create(MINTCLIENT::Message::RoutingServerDisconnect).Send(socket);
        }

        // Make sure the socket is closed
        socket.Close();

        delete packetBuffer;

        while (!commands.IsEmpty())
        {
            commands.GetHead()->DropFromClient();
        }

        StyxNet::RemoveClient();
    }

    //
    // Trigger shutdown sequence.
    // - 1. `eventQuit` is signaled.
    // - 2. `ThreadProc` picks up the command then closes its loop.
    // - 3. `ThreadProc` calls `delete` on the `MINTCLIENT` attached to its context.
    // - 4. `~Client()` is performed.
    //
    void Client::Shutdown()
    {
        this->eventQuit.Signal();
    }

    void Client::QueueCommand(MINTCommand* command, bool notify)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::QueueCommand -> " << HEX(command->command_id, 8) << " Notify: -> " << notify);

        commandsCrit.Enter();
        this->commands.Append(command);
        commandsCrit.Exit();

        if (notify) { eventCommand.Signal(); }
    }

    //
    // Search for command by id
    // Remove from client.
    //
    void Client::DropCommand(MINTCommand* command)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::DropCommand -> dropping [" << HEX(command->command_id, 8) << "] from this client...");

        this->commands.Unlink(command);

        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::DropCommand -> [" << HEX(command->command_id, 8) << "] -> dropped!");
    }

    MINTCLIENT::Client::MINTCommand* MINTCLIENT::Client::GetCommandById(CRC command_id)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::GetCommand -> getting [" << HEX(command_id, 8) << "] from this client...");

        Client::MINTCommand* ret = nullptr;

        commandsCrit.Enter();

        for (U32 i = 0; i < this->commands.GetCount(); i++)
        {
            auto cc = this->commands[i];
            if (cc->command_id == command_id)
            {
                ret = cc;
            }
        }

        if (!ret)
        {
            LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] >>> COULD NOT FIND [" << HEX(command_id, 8) << "] in command queue for this MINTCLIENT.");
        }
        else
        {
            LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] >>> FOUND [" << HEX(command_id, 8) << "] in command queue for this MINTCLIENT.");
        }

        commandsCrit.Exit();

        return ret;
    }

    bool MINTCLIENT::Client::HandlesCommandId(CRC command_id)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::HasCommand -> [" << HEX(command_id, 8) << "] -> Searching...");

        bool did_find = false;

        if (this->GetCommandById(command_id))
        {
            did_find = true;
        }

        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::HasCommand -> [" << HEX(command_id, 8) << "] -> " << (did_find ? "Found" : "Not Found"));

        return did_find;
    }

    void _DebugShowMissingContext(const StyxNet::Packet& packet, const Client* client)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::_DebugShowMissingContext " << Message::GetCommandString(packet.GetCommand()) << " packet found but no command was queued to handle it.");
    }

    //
    // Client::ThreadProc
    //
    // Thread procedure
    //
    U32 STDCALL Client::MINTMasterThread(void* context)
    {
        LOG_DIAG((">>> MINTCLIENT::Client::ThreadProc master MINTCLIENT thread starts here."));

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
        events.AddEvent(client->event, nullptr);
        events.AddEvent(client->eventQuit, &client);
        events.AddEvent(client->eventCommand, &client->commands);

        U32 nextTime = Clock::Time::Ms();

        while (!quit)
        {
            void* eventContext;

            S32 remaining = nextTime - Clock::Time::Ms();

            if (remaining > 0 && events.Wait(eventContext, FALSE, remaining))
            {
                if (!eventContext)
                {
                    Win32::Socket::NetworkEvents networkEvents;
                    client->socket.EnumEvents(client->event, networkEvents);

                    if (networkEvents.GetEvents() & FD_CONNECT)
                    {
                        S32 error = networkEvents.GetError(FD_CONNECT_BIT);
                        if (error)
                        {
                            LDIAG(">>> Connection to server failed...");
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
                            client->HandleIncomingPacket(*pkt);
                        }
                    }
                }
                else
                {
                    // A command queued with the event system.
                    if (eventContext == &client->commands)
                    {
                        // If we're connected, send off the command else requeue.
                        if ((client->flags & StyxNet::ClientFlags::Connected) == StyxNet::ClientFlags::Connected)
                        {
                            for (U32 i = 0; i < client->commands.GetCount(); i++)
                            {
                                MINTCommand* cc = client->commands[i];
                                if (!cc->listener_only)
                                {
                                    cc->AttemptSend();
                                }
                            }
                        }
                        else
                        {
                            // Not connected yet, requeue the command.
                            client->eventCommand.Signal();
                            nextTime += 500;
                        }
                    }
                    else if (eventContext == &client)
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

        LOG_DIAG((">>> MINTCLIENT::Client::ThreadProc master MINTCLIENT thread has finished..."));

        return (0x6666);
    }

    //
    // Handle incoming packet, route to appropriate handler.
    //
    void MINTCLIENT::Client::HandleIncomingPacket(const StyxNet::Packet& packet)
    {
        LDIAG("[" << HEX(GetCurrentThreadId(), 8) << "] MINTCLIENT::Client::ProcessingIncomingPacket [" << HEX(packet.GetCommand(), 8) << "] (" << Message::GetCommandString(packet.GetCommand()) << ")");

        switch (packet.GetCommand())
        {
            case MINTCLIENT::Message::ServerConnect: // 0x433AB32B
            {
                LDIAG(">>> Connection established with server!");
                flags |= StyxNet::ClientFlags::Connected;
            }
            break;

            // For a known packet, if it's the response to a command, pull up command context and deal with it.
            case MINTCLIENT::Message::DirectoryListServers: // 0x77712BCE
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::DirectoryListRooms: // 0x910DB9D4
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::IdentityCreate: // 0x14096404
            case MINTCLIENT::Message::IdentityAuthenticate: // 0xCD5AF72B
            {
                const auto r = *(MINTCLIENT::Identity::Result*)(packet.GetData());

                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataFromStruct(r);
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerRoomConnect: // 0xEE37226B
            case MINTCLIENT::Message::RoutingServerRoomRegister: // 0x59938A8D
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerGetUserList: // 0x82E37940
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerBroadcastChat: // 0xC79C5EB4
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerUserEnter: // 0x75BBABEE
            case MINTCLIENT::Message::RoutingServerUserLeave: // 0xCF1E785F
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerUpdateGame: // 0xB04C290F
            case MINTCLIENT::Message::RoutingServerCreateGame: // 0x2A0FB0FD
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerGetGameList: // 0x41D50E29
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerGameCreated:
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->SetDataBytes(packet.GetData(), packet.GetLength());
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }
            }
            break;

            case MINTCLIENT::Message::RoutingServerDisconnect: // 0xF9CE798B
            case MINTCLIENT::Message::ServerShutdown: // 0xD26E9A5C
            case MINTCLIENT::Message::ServerDisconnect: // 0x8542A47A
            {
                auto* cmd = this->GetCommandById(packet.GetCommand());

                if (cmd)
                {
                    cmd->Done();
                }
                else
                {
                    _DebugShowMissingContext(packet, this);
                }

                eventQuit.Signal();
            }
            break;

            default:
                // Unknown packet command
                LDIAG("Unknown Packet Command " << HEX(packet.GetCommand(), 8) << " from server");
                break;
        }
    }
}

void MINTInitialize()
{
    // Get this library ready for use.
}

void MINTTerminate()
{
    // Shutdown this library.
}
