#pragma once

#include "../system/defines.h"
#include "../system/std.h"

#include "../styxnet/styxnet_packet.h"
#include "../styxnet/styxnet_eventqueue.h"
#include "../styxnet/win32_thread.h"

#include <list>
#include <vector>
#include <string>

#include "Errors.h"


void WONInitialize();
void WONTerminate();


namespace MINTCLIENT
{
    struct Identity;
    struct Directory;

    namespace Message
    {
        static const unsigned int ServerConnect = 0x433AB32B;                  // MINTCLIENT::Message::ServerConnect
        static const unsigned int ServerShutdown = 0xD26E9A5C;                 // MINTCLIENT::Message::ServerShutdown

        static const unsigned int IdentityAuthenticate = 0xCD5AF72B;           // MINTCLIENT::Message::IdentityAuthenticate

        static const unsigned int DirectoryListServers = 0x77712BCE;           // MINTCLIENT::Message::DirectoryListServers
        static const unsigned int DirectoryListRooms = 0x910DB9D4;             // MINTCLIENT::Message::DirectoryListRooms

        static const unsigned int RoutingServerRoomConnect = 0xEE37226B;       // MINTCLIENT::Message::RoutingServerRoomConnect
        static const unsigned int RoutingServerRoomRegister = 0x59938A8D;      // MINTCLIENT::Message::RoutingServerRoomRegister

        static const unsigned int RoutingServerGetNumUsers = 0xACCD008F;       // MINTCLIENT::Message::RoutingServerGetNumUsers
        static const unsigned int RoutingServerGetUserAddress = 0x24B1E0F;     // MINTCLIENT::Message::RoutingServerGetUserAddress
        static const unsigned int RoutingServerGetUserList = 0x82E37940;       // MINTCLIENT::Message::RoutingServerGetUserList

        static const unsigned int RoutingServerBroadcastChat = 0xC79C5EB4;     // MINTCLIENT::Message::RoutingServerBroadcastChat
        static const unsigned int RoutingServerWhisperChat = 0x1A6C1A40;       // MINTCLIENT::Message::RoutingServerWhisperChat

        static const unsigned int RoutingServerCreateGame = 0x2A0FB0FD;        // MINTCLIENT::Message::RoutingServerCreateGame
        static const unsigned int RoutingServerUpdateGame = 0xB04C290F;        // MINTCLIENT::Message::RoutingServerUpdateGame
        static const unsigned int RoutingServerDeleteGame = 0xDDCA3C97;        // MINTCLIENT::Message::RoutingServerDeleteGame

        static const unsigned int RoutingServerDisconnect = 0xF9CE798B;        // MINTCLIENT::Message::RoutingServerDisconnect

    }

    namespace IPSocket
    {
        struct Address
        {
            char* ip;
            int port;
            char* addr;

            Address(const char* address)
            {
                char* next_token;

                addr = new char[strlen(address)];
                memcpy_s(addr, strlen(address), address, strlen(address));

                char* ip = strtok_s(addr, ":", &next_token);
                int port = atoi(strtok_s(nullptr, ":", &next_token));

                this->ip = ip;
                this->port = port;
            };

            Address() {};

            ~Address()
            {
                // delete addr;
            };

            // MINTCLIENT::IPSocket::Address FromString(const char* address);

            char* GetAddressString(bool andPort)
            {
                char buf[128];
                Utils::Strcpy(buf, this->ip);
                //Utils::Strcat(buf, Utils::ItoA(this->port, buf, 10));
                return buf;
            }
        };
    }

    //
    // Response sent to server.
    //
    struct CommandRequest
    {
        void* context;
    };

    //
    // Response sent back from server.
    //
    struct CommandResult
    {
        WONAPI::Error error = WONAPI::Error_Failure;
        StrCrc<256> message;
        void* context;
    };

    class Client : public StyxNet::EventQueue
    {
    public:
        struct Config
        {
            // Address of the server to connect to
            Win32::Socket::Address address;

            Config(const Win32::Socket::Address& address) : address(address)
            {
            }

            Config(const MINTCLIENT::IPSocket::Address& address)
            {
                this->address = Win32::Socket::Address(address.ip, U16(address.port));
            }

            Config(const char* address)
            {
                auto addr = IPSocket::Address(address);
                this->address = Win32::Socket::Address(addr.ip, U16(addr.port));
            }
        };

        struct CommandContext
        {
            // The client attached to this command.
            Client* client;

            // The context that spawned this command.
            void* context;

            // Command to send off to the server.
            CRC command;
            U8* cmd_data;
            size_t data_size;
            Bool wasProcessed;

            // Signal that we're done processing this command.
            Win32::EventIndex commandDone;
            Win32::EventIndex abort;

            int times_called = 0;

            template <class DATA> void SetData(const DATA& data)
            {
                this->data_size = sizeof(DATA);
                this->cmd_data = new U8[data_size];
                memcpy(this->cmd_data, &data, this->data_size);

                LOG_DEV(("CommandContext->SetData %i", times_called++));
                Utils::MemoryDump(this->cmd_data, this->data_size);
            }

            template <class CONTEXT> void SetContext(const CONTEXT& context)
            {
                this->context = context;
            }

            Bool Send()
            {
                // Instantiate a `Packet`.
                StyxNet::Packet& pkt = StyxNet::Packet::Create(
                    command,
                    data_size
                );

                // Get pointer to `data` of the packet.
                U8* pkt_d = pkt.GetData();
                // Copy `cmd_data` to the `data` of the packet.
                Utils::Memcpy(pkt_d, cmd_data, data_size);

                // Send the packet across the network.
                return pkt.Send(client->socket);
            }

            CommandContext(Client* client) : client(client), data_size(0), wasProcessed(0)
            {
            }

            ~CommandContext()
            {
                if (this->data_size > 0) {
#ifdef _DEBUG
                    Utils::MemoryDump(cmd_data, this->data_size);
                    LOG_DEV(("Cleaning up. (~CommandContext)."))
#endif
                    delete[] cmd_data;
                }
            }
        };

        template <class T> struct ContextList
        {
            Win32::EventIndex::List<32> events;
            std::vector<CommandContext> contexts;

            void (*callback)(const T& result);

            void Add(CommandContext* ctx)
            {
                contexts.push_back(*ctx);
                events.AddEvent(ctx->commandDone, ctx);
            }
        };

        Client(const MINTCLIENT::Client::Config& config);
        ~Client();
        void Shutdown();

        void SendCommand(CommandContext* context);

        CommandContext* GetCommandContext()
        {
            return commandContext;
        }

    private:
        // Client configuration
        MINTCLIENT::Client::Config config;

        // Flags
        U32 flags;

        // Client Thread
        Win32::Thread thread;

        // Event to stop the client
        Win32::EventIndex eventQuit;

        // Event that there's a custom command incoming.
        Win32::EventIndex eventCommand;
        CommandContext* commandContext;

        // Thread procedure
        static U32 STDCALL ThreadProc(void* context);

        // Handle an incoming packet
        void ProcessPacket(const StyxNet::Packet& packet);

        // Address of the server
        Win32::Socket::Address address;

        // Socket which connects our client to the server
        Win32::Socket socket;

        // Event handle for the above socket
        Win32::EventIndex event;

        // Buffer for receiving data
        StyxNet::Packet::Buffer* packetBuffer;

        // Index
        U32 index;
    };
}
