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


namespace WONAPI
{
    namespace IPSocket
    {
        struct Address
        {
            char* ip;

            Address(const char* s)
            {
                ip = new char[sizeof(*s)];
                memcpy(ip, s, strlen(s));
            };

            Address() : ip(0) {};

            ~Address()
            {
            };
        };
    }

    struct DirEntityListResult
    {
        Error error;
    };

    struct DetectFirewallResult
    {
        //
    };

    struct StartTitanServerResult
    {
        //
    };

    struct RoutingServerClient
    {
        const U32 GROUPID_ALLUSERS = 0;

        RoutingServerClient()
        {
            //
        }

        struct RegisterClientResult
        {
            //
        };

        struct GetClientListResult
        {
            //
        };

        struct ReadDataObjectResult
        {
            //
        };

        struct GetNumUsersResult
        {
            //
        };

        struct ClientDataWithReason
        {
            //
        };

        struct ClientIdWithReason
        {
            //
        };

        struct ClientIdWithFlag
        {
            //
        };

        struct ClientId
        {
            //
        };

        struct DataObjectWithLifespan
        {
            //
        };

        struct DataObject
        {
            //
        };

        struct DataObjectModification
        {
            //
        };

        struct ASCIIChatMessage
        {
            //
        };

        struct UnicodeChatMessage
        {
            //
        };

        struct RawChatMessage
        {
            //
        };

        void InstallClientEnterExCatcherEx(void* callback, void* p)
        {
            //
        }

        void GetNumUsersEx(unsigned short theTag, void* callback, void* privsType)
        {
        }

        void Close()
        {
            //
        }
    };
}

namespace WONCommon
{
    struct DataObjectTypeSet
    {
        void insert()
        {

        }
    };

    struct DataObject
    {

    };

    struct RawBuffer
    {
        void assign()
        {
            //
        }
    };
}

namespace WONMsg
{
    namespace MMsgRoutingGetClientListReply
    {
        struct ClientData
        {
            std::wstring mClientName;
            U32 mClientId;
            Bool mIsModerator;
            Bool mIsMuted;
            long mIPAddress;
        };
    }

    enum
    {
        StatusCommon_Success = 0,

        StatusRouting_ObjectAlreadyExists,

        StatusAuth_InvalidCDKey = 5,
    };
}

void WONInitialize();
void WONTerminate();


namespace MINTCLIENT
{
    struct Identity;

    namespace IPSocket
    {
        struct Address
        {
            char* ip;
            int port;

            Address(const char* address)
            {
                char* next_token;

                char* addr = new char[strlen(address)];
                memcpy_s(addr, strlen(address), address, strlen(address));

                char* ip = strtok_s(addr, ":", &next_token);
                int port = atoi(strtok_s(nullptr, ":", &next_token));

                this->ip = ip;
                this->port = port;
            };
            ~Address() {};

            MINTCLIENT::IPSocket::Address FromString(const char* address);
        };
    }

    struct CommandResult
    {
        WONAPI::Error error = WONAPI::Error_Failure;
    };

    struct DirectoryEntity
    {
        std::wstring mName;
    };

    typedef std::list<DirectoryEntity> DirectoryEntityList;

    struct DirectoryResult : CommandResult
    {
        DirectoryEntityList entityList;
    };

    WONAPI::Error GetDirectoryEx(MINTCLIENT::Identity* identity, const std::vector<MINTCLIENT::IPSocket::Address>* servers, void (*callback)(const DirectoryResult& result));

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

            Config(const char* address)
            {
                auto addr = IPSocket::Address(address);
                this->address = Win32::Socket::Address(addr.ip, U16(addr.port));
            }
        };

        struct CommandContext
        {
            // Command to send off to the server.
            CRC command;
            // Signal that we're done processing this command.
            Win32::EventIndex commandDone;
            // The client attached to this command.
            Client* client;

            CommandContext(Client* client) : client(client)
            {
            }
        };


        template <class T>
        struct ContextList
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
