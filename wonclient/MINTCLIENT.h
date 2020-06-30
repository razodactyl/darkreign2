#pragma once

#include "../system/defines.h"
#include "../system/std.h"

#include "../styxnet/styxnet_packet.h"
#include "../styxnet/styxnet_eventqueue.h"
#include "../styxnet/win32_thread.h"
#include "../styxnet/win32_critsec.h"

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

    static const unsigned int DefaultTimeout = 5000;

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
        static const unsigned int ServerDisconnect = 0x8542A47A;               // MINTCLIENT::Message::ServerDisconnect

        inline const char* GetCommandString(U32 command_id)
        {
            switch (command_id)
            {
                case MINTCLIENT::Message::DirectoryListServers:
                    return "MINTCLIENT::Message::DirectoryListServers";

                case MINTCLIENT::Message::DirectoryListRooms:
                    return "MINTCLIENT::Message::DirectoryListRooms";

                case MINTCLIENT::Message::IdentityAuthenticate:
                    return "MINTCLIENT::Message::IdentityAuthenticate";

                case MINTCLIENT::Message::RoutingServerRoomConnect:
                    return "MINTCLIENT::Message::RoutingServerRoomConnect";

                case MINTCLIENT::Message::RoutingServerRoomRegister:
                    return "MINTCLIENT::Message::RoutingServerRoomRegister";

                case MINTCLIENT::Message::RoutingServerGetUserList:
                    return "MINTCLIENT::Message::RoutingServerGetUserList";

                case MINTCLIENT::Message::RoutingServerBroadcastChat:
                    return "MINTCLIENT::Message::RoutingServerBroadcastChat";

                case MINTCLIENT::Message::RoutingServerCreateGame:
                    return "MINTCLIENT::Message::RoutingServerCreateGame";

                case MINTCLIENT::Message::RoutingServerDisconnect:
                    return "MINTCLIENT::Message::RoutingServerDisconnect";

                case MINTCLIENT::Message::ServerShutdown:
                    return "MINTCLIENT::Message::ServerShutdown";

                case MINTCLIENT::Message::ServerConnect:
                    return "MINTCLIENT::Message::ServerConnect";

                default:
                {
                    char* info = new char[64];
                    char* hh = new char[10];
                    Utils::Sprintf(info, 32, "UnknownCommand [%s]", Utils::StrFmtHex(hh, 8, command_id));
                    return info;
                }
                break;
            }
        }

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

        template <class R> struct MINTWrapper
        {
            void (*callback)(const R& result);
        };

        template <class T> struct MINTCallback
        {
            void* _wrapped;

            template <class R> MINTCallback* operator= (void (*callback)(const R& other))
            {
                auto w = new MINTWrapper<R>();
                w->callback = callback;

                _wrapped = w;

                return this;
            }

            template <class R> void operator() (const R& other)
            {
                auto w = (MINTWrapper<R>*)this->_wrapped;
                w->callback(other);
            }
        };

        struct MINTCommand
        {
        private:
            Win32::EventIndex _done;        // Signal that there's data waiting to be processed.
            Win32::EventIndex _abort;       // Signal to abort.
            Win32::EventIndex _timeout;     // Signal to timeout.

        public:
            Client* client;                 // Client attached to this command.
            void* context;                  // Context specified by API caller.

            CRC command_id;                 // Associated command 0x11223344 sent to / from server, e.g MINTCLIENT::Message::ServerShutdown
            U8* cmd_data;                   // BYTES associated with this command, in request or response.
            size_t data_size;
            Bool recycle;                   // Specify that we want to re-queue this command once we've dealt with it.
            Bool listener_only;             // Specify that we only want to use this context as a way for dealing with incoming messages from the server.=

            bool did_pass;                  // Set when the command has been passed to a client, prevents the command from being passed again.
            bool did_send;                  // Set when the `Send` function has been called. Further sends are prevented. A `MINTCommand` should be dropped once it has been sent and used up.
            bool did_complete;              // Set when `done` is signaled.
            bool did_abort;                 // Set when 'abort' is signaled.
            bool did_timeout;               // Set when 'timeout' is signaled.

            U32 times_called = 0;           // Count of times this item has been processed.

            MINTCallback<class T> callback;

            void Done()
            {
                // Reset signal.
                _done.Wait(0);
                // (Re)signal.
                _done.Signal();
                this->did_complete = true;
            }

            void Abort()
            {
                // Reset signal.
                _abort.Wait(0);
                // (Re)signal.
                _abort.Signal();
                this->did_abort = true;
            }

            void Timeout()
            {
                // Reset signal.
                _timeout.Wait(0);
                // (Re)signal.
                _timeout.Signal();
                this->did_timeout = true;
            }

            Bool Finished()
            {
                return did_timeout || did_complete || did_abort;
            }

            // template <class T> void RegisterEvents(T* event_list)
            // {
            //     event_list->AddEvent(*(this->_done), this);
            //     event_list->AddEvent(*(this->_abort), this);
            //     event_list->AddEvent(*(this->_timeout), this);
            // }

            void ExtractEvents(Win32::EventIndex::List<32>* event_list)
            {
                (*event_list).AddEvent(_done, this);
                (*event_list).AddEvent(_abort, this);
                (*event_list).AddEvent(_timeout, this);
            }

            //
            // Given a struct containing information, determine required size and copy over this command's data.
            //
            template <class DATA> void SetDataFromStruct(const DATA& data)
            {
                this->data_size = sizeof(DATA);
                this->cmd_data = new U8[data_size];
                Utils::Memcpy(this->cmd_data, &data, this->data_size);
            }

            //
            // Just set the raw U8 bytes to be decoded by a processor.
            //
            void SetDataBytes(const U8* data, size_t len)
            {
                this->data_size = len;
                this->cmd_data = new U8[data_size];
                Utils::Memcpy(this->cmd_data, data, this->data_size);
            }

            template <class CONTEXT> void SetContext(const CONTEXT& context)
            {
                this->context = context;
            }

            WONAPI::Error GetError()
            {
                return this->did_abort ? WONAPI::Error_Aborted : this->did_timeout ? WONAPI::Error_Timeout : WONAPI::Error_Success;
            }

            Bool ClientHandlesCommand(CRC command)
            {
                if (this->client->HasCommand(command))
                {
                    return true;
                }

                return false;
            }

            //
            // Tell the attached client to drop / stop handling this command.
            //
            void DropFromClient()
            {
                ASSERT(client);
                client->DropCommand(this);
            }

            Bool AttemptSend()
            {
                if (!did_send) {
                    did_send = true;

                    // Instantiate a `Packet`.
                    StyxNet::Packet& pkt = StyxNet::Packet::Create(
                        command_id,
                        data_size
                    );

                    // Get pointer to `data` of the packet.
                    U8* pkt_d = pkt.GetData();

                    // Copy `cmd_data` to the `data` of the packet.
                    if (data_size > 0) {
                        Utils::Memcpy(pkt_d, cmd_data, data_size);
                    }

                    // Send the packet across the network.
                    return pkt.Send(client->socket);
                }

                return false;
            }

            MINTCommand()
            {
            };

            MINTCommand(Client* client)
            {
                this->client = client;
                this->context = nullptr;

                this->command_id = 0x00000000;
                this->cmd_data = nullptr;
                this->data_size = 0;
                this->recycle = false;
                this->listener_only = false;

                this->did_pass = false;
                this->did_send = false;
                this->did_complete = false;
                this->did_abort = false;
                this->did_timeout = false;

                this->times_called = 0;
            }

            ~MINTCommand()
            {
                if (this->data_size > 0) {
#ifdef _DEBUG
                    Utils::MemoryDump(cmd_data, this->data_size);
                    LOG_DEV(("~MINTCommand -> command is going away..."));
#endif
                    delete[] cmd_data;
                }

                this->DropFromClient();
            }
        };

        struct CommandList
        {
        private:
            // Win32::EventIndex::List<16> _events;
            // Win32::CritSec _events_critical;

            std::vector<MINTCommand*> _commands;
            Win32::CritSec _commands_critical;

        public:

#define MINT_MAX_EVENTS 32

            Win32::EventIndex::List<MINT_MAX_EVENTS>* GetEvents()
            {
                Win32::EventIndex::List<MINT_MAX_EVENTS>* events = new Win32::EventIndex::List<MINT_MAX_EVENTS>();

                _commands_critical.Enter();

                int count = 0;

                for (auto& command : _commands)
                {
                    // Extract `command._events` to `events`.
                    command->ExtractEvents(events);
                    count += 3;
                }

                ASSERT(count < MINT_MAX_EVENTS);

                _commands_critical.Exit();

                return events;
            }

            void Add(MINTCommand* cmd)
            {
                _commands_critical.Enter();

                _commands.push_back(&*cmd);

                _commands_critical.Exit();
            }

            void Remove(MINTCommand* cmd)
            {
                _commands_critical.Enter();

                std::vector<MINTCommand*> keep;

                for (auto* command : _commands)
                {
                    if (cmd != command)
                    {
                        keep.push_back(&*command);
                    }
                    else
                    {
                        LOG_DEV(("Removing command."));
                        command->DropFromClient();
                    }
                }

                _commands.clear();
                for (auto* command : keep)
                {
                    _commands.push_back(&*command);
                }

                _commands_critical.Exit();
            }

            void RemoveAll()
            {
                for (auto* command : _commands)
                {
                    this->Remove(command);
                }
            }

            std::vector<MINTCommand*> GetAll()
            {
                std::vector<MINTCommand*> current;

                _commands_critical.Enter();

                for (auto* command : _commands)
                {
                    current.push_back(&*command);
                }

                _commands_critical.Exit();

                return current;
            }

            void PassToClient()
            {
                _commands_critical.Enter();

                for (auto* command : _commands)
                {
                    ASSERT(command->client);

                    if (!command->did_pass)
                    {
                        command->did_pass = true;
                        command->client->QueueCommand(command, false);

                        if (command->ClientHandlesCommand(command->command_id))
                        {
                            command->client->eventCommand.Signal();
                        }
                        else
                        {
                            ASSERT(false == true);
                        }
                    }
                }

                _commands_critical.Exit();
            }

            void ShutdownClients()
            {
                _commands_critical.Enter();

                for (auto* command : _commands)
                {
                    ASSERT(command->client);
                    command->client->Shutdown();
                }

                _commands_critical.Exit();
            }

            bool HasCommand(CRC command_id)
            {
                _commands_critical.Enter();

                bool ret = false;

                for (auto& _command : _commands)
                {
                    if (_command->command_id == command_id)
                    {
                        ret = true;
                    }
                }

                _commands_critical.Exit();

                return ret;
            }

            void TimeoutAll()
            {
                _commands_critical.Enter();

                for (auto& _command : _commands)
                {
                    _command->Timeout();
                }

                _commands_critical.Exit();
            }

            void AbortAll()
            {
                _commands_critical.Enter();

                for (auto& _command : _commands)
                {
                    _command->Abort();
                }

                _commands_critical.Exit();
            }

            Bool Busy()
            {
                _commands_critical.Enter();

                bool ret = false;

                for (auto& _command : _commands)
                {
                    if (!_command->Finished())
                    {
                        // Hasn't been processed yet.
                        ret = true;
                    }
                }

                _commands_critical.Exit();

                // All commands have a return status.
                return ret;
            }

            ~CommandList()
            {
                LOG_DEV((">>> Cleaning up a `CommandList`..."));
                this->RemoveAll();
            }
        };

        Client(const MINTCLIENT::Client::Config& config);
        ~Client();

        void Shutdown();

        void QueueCommand(MINTCommand* command, bool notify = true);
        void DropCommand(MINTCommand* command);

        MINTCommand* GetCommand(CRC command_id);
        bool HasCommand(CRC command_id);

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

        // SafeQueue<MINTCommand*, 32> commands;
        List<MINTCommand> commands;

        // Thread procedure
        static U32 STDCALL MINTMainThread(void* context);

        // Handle an incoming packet
        void ProcessIncomingPacket(const StyxNet::Packet& packet);

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
