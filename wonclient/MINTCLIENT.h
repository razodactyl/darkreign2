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


namespace Win32 {
    namespace DNS {
        class Host;
    }
}

void MINTInitialize();
void MINTTerminate();


namespace MINTCLIENT
{
    struct Identity;
    struct Directory;

    static const unsigned int DefaultTimeout = 10000;

    namespace Error
    {
        static const unsigned int IdentityAuthenticationFailed = 0x13BD404E;    // MINTCLIENT::Error::IdentityAuthenticationFailed
        static const unsigned int IdentityAlreadyExists = 0x4D6ABC5b;           // MINTCLIENT::Error::IdentityAlreadyExists

        inline const char* GetErrorString(U32 error_id)
        {
            switch (error_id)
            {
                case MINTCLIENT::Error::IdentityAuthenticationFailed:
                    return "MINTCLIENT::Error::IdentityAuthenticationFailed";
                case MINTCLIENT::Error::IdentityAlreadyExists:
                    return "MINTCLIENT::Error::IdentityAlreadyExists";

                default:
                {
                    char* info = new char[64];
                    char* hh = new char[10];
                    Utils::Sprintf(info, 32, "Unknown Error [%s]", Utils::StrFmtHex(hh, 8, error_id));
                    return info;
                }
                break;
            }
        }
    }

    namespace Message
    {
        static const unsigned int ServerConnect = 0x433AB32B;                   // MINTCLIENT::Message::ServerConnect
        static const unsigned int ServerShutdown = 0xD26E9A5C;                  // MINTCLIENT::Message::ServerShutdown

        static const unsigned int IdentityCreate = 0x14096404;                  // MINTCLIENT::Message::IdentityCreate
        static const unsigned int IdentityAuthenticate = 0xCD5AF72B;            // MINTCLIENT::Message::IdentityAuthenticate

        static const unsigned int DirectoryListServers = 0x77712BCE;            // MINTCLIENT::Message::DirectoryListServers
        static const unsigned int DirectoryListRooms = 0x910DB9D4;              // MINTCLIENT::Message::DirectoryListRooms

        static const unsigned int RoutingServerRoomConnect = 0xEE37226B;        // MINTCLIENT::Message::RoutingServerRoomConnect
        static const unsigned int RoutingServerRoomRegister = 0x59938A8D;       // MINTCLIENT::Message::RoutingServerRoomRegister

        static const unsigned int RoutingServerUserEnter = 0x75BBABEE;          // MINTCLIENT::Message::RoutingServerUserEnter
        static const unsigned int RoutingServerUserLeave = 0xCF1E785F;          // MINTCLIENT::Message::RoutingServerUserLeave

        static const unsigned int RoutingServerGetNumUsers = 0xACCD008F;        // MINTCLIENT::Message::RoutingServerGetNumUsers
        static const unsigned int RoutingServerGetUserAddress = 0x24B1E0F;      // MINTCLIENT::Message::RoutingServerGetUserAddress
        static const unsigned int RoutingServerGetUserList = 0x82E37940;        // MINTCLIENT::Message::RoutingServerGetUserList

        static const unsigned int RoutingServerBroadcastChat = 0xC79C5EB4;      // MINTCLIENT::Message::RoutingServerBroadcastChat
        static const unsigned int RoutingServerWhisperChat = 0x1A6C1A40;        // MINTCLIENT::Message::RoutingServerWhisperChat

        static const unsigned int RoutingServerCreateGame = 0x2A0FB0FD;         // MINTCLIENT::Message::RoutingServerCreateGame
        static const unsigned int RoutingServerUpdateGame = 0xB04C290F;         // MINTCLIENT::Message::RoutingServerUpdateGame
        static const unsigned int RoutingServerDeleteGame = 0xDDCA3C97;         // MINTCLIENT::Message::RoutingServerDeleteGame
        static const unsigned int RoutingServerGetGameList = 0x41D50E29;       // MINTCLIENT::Message::RoutingServerGetGameList

        static const unsigned int RoutingServerGameCreated = 0xA18A092D;        // MINTCLIENT::Message::RoutingServerGameCreated

        static const unsigned int RoutingServerDisconnect = 0xF9CE798B;         // MINTCLIENT::Message::RoutingServerDisconnect
        static const unsigned int ServerDisconnect = 0x8542A47A;                // MINTCLIENT::Message::ServerDisconnect

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
                    Utils::Sprintf(info, 32, "Unknown Command [%s]", Utils::StrFmtHex(hh, 8, command_id));
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
            char* host;
            char* port;
            char* text;

            Address(const char* address)
            {
                char* next_token;

                auto *const _addr = new char[strlen(address) + 1];
                memset(_addr, 0, strlen(address) + 1);
                memcpy_s(_addr, strlen(address), address, strlen(address));
                char* host = Utils::Strdup(strtok_s(_addr, ":", &next_token));
                char* port = Utils::Strdup(strtok_s(nullptr, ":", &next_token));
                delete[] _addr;

                this->host = host;
                this->port = port;
                this->text = Utils::Strdup(address);
            };

            Address(char* host, char* port)
            {
                this->host = Utils::Strdup(host);
                this->port = Utils::Strdup(port);

                this->text = Utils::Strcat(Utils::Strdup(host), ":");
                this->text = Utils::Strcat(this->text, this->port);
            }

            Address() {};
            ~Address() {};

            char* GetText()
            {
                return text;
            }

            char* GetHost()
            {
                return host;
            }

            char* GetPort()
            {
                return port;
            }

            int GetPortI()
            {
                return atoi(this->port);
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
            // This needs to be a resolved `Address` for the `MINTMasterThread` to connect to.
            Win32::Socket::Address address;
            char* host;
            char* port;
            IPSocket::Address _address;

            Config(const char* address)
            {
                auto c = MINTCLIENT::IPSocket::Address(address);
                host = Utils::Strdup(c.host);
                port = Utils::Strdup(c.port);
                _address = c;
            }

            Config(const MINTCLIENT::IPSocket::Address& address)
            {
                host = Utils::Strdup(address.host);
                port = Utils::Strdup(address.port);
                _address = address;
            }
        };

        //
        // Template tricks for dynamically wrapping a callback.
        //

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

        //
        //
        //

        struct MINTBuffer
        {
            U8* data;
            U32 size;

            void assign(const U8* data, U32 size)
            {
                this->data = new U8[size];
                this->size = size;
                Utils::Memcpy(this->data, data, size);
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

            int ExtractEvents(Win32::EventIndex::List<32>* event_list)
            {
                (*event_list).AddEvent(_done, this);
                (*event_list).AddEvent(_abort, this);
                (*event_list).AddEvent(_timeout, this);

                return 3; // _done, _abort, _timeout.
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

            void AddDataBytes(const U8* data, size_t len)
            {
                // Create buffer for previous data + new data.
                U8* extended_data = new U8[this->data_size + len];
                // Copy previous memory to new location.
                Utils::Memcpy(extended_data, this->cmd_data, this->data_size);

                // Free original memory.
                delete this->cmd_data;

                // Copy extended data to new location.
                Utils::Memcpy(extended_data + this->data_size, data, len);

                // Update the new data size.
                this->data_size = this->data_size + len;

                // Replace original data.
                this->cmd_data = extended_data;
            }

            template <class CONTEXT> void SetContext(const CONTEXT& context)
            {
                this->context = context;
            }

            WONAPI::Error GetError()
            {
                // Catch obvious errors.
                if (this->did_abort || this->did_timeout)
                {
                    return this->did_abort ? WONAPI::Error_Aborted : this->did_timeout ? WONAPI::Error_Timeout : WONAPI::Error_GeneralFailure;
                }

                // Catch response errors.
                if (this->data_size > 3)
                {
                    U32 err_val = this->cmd_data[3] << 24 | this->cmd_data[2] << 16 | this->cmd_data[1] << 8 | this->cmd_data[0];

                    switch (this->command_id)
                    {
                        case MINTCLIENT::Message::IdentityAuthenticate:
                        {
                            if (err_val == MINTCLIENT::Error::IdentityAuthenticationFailed) { return MINTCLIENT::Error::IdentityAuthenticationFailed; }
                        }
                        break;

                        case MINTCLIENT::Message::IdentityCreate:
                        {
                            if (err_val == MINTCLIENT::Error::IdentityAlreadyExists) { return MINTCLIENT::Error::IdentityAlreadyExists; }
                        }
                        break;

                        default:
                            break;
                    }
                }

                // No errors encountered, all good!
                return WONAPI::Error_Success;
            }

            Bool ClientHandlesCommandId(CRC command_id)
            {
                ASSERT(this->client);
                return this->client->HandlesCommandId(command_id);
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

            // MINTCommand()
            // {
            // };

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
                    count += command->ExtractEvents(events);
                }

                ASSERT(count < MINT_MAX_EVENTS);

                _commands_critical.Exit();

                return events;
            }

            //
            // Add a `MINTCommand` to this list of commands.
            //
            void Add(MINTCommand* cmd)
            {
                _commands_critical.Enter();
                _commands.push_back(&*cmd);
                _commands_critical.Exit();
            }

            //
            // Create and return a list of pointers to the current list of commands.
            //
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

            //
            // Remove from this list of commands.
            //
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

            //
            // Iterate and remove all.
            //
            void RemoveAll()
            {
                for (auto* command : _commands)
                {
                    this->Remove(command);
                }
            }

            bool HasCommandId(CRC command_id)
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

            //
            // Iterate through commands and send them to their attached client.
            //
            void PassToClient()
            {
                _commands_critical.Enter();

                for (auto* command : _commands)
                {
                    ASSERT(command->client);

                    if (command->did_pass)
                    {
                        LDIAG("MINTCLIENT::CommandList::PassToClient -> Attempting to pass `" << Message::GetCommandString(command->command_id) << "` to client when `did_pass = true`!!!");
                    }

                    // Only if we haven't passed it on previously.
                    if (!command->did_pass)
                    {
                        command->did_pass = true;
                        command->client->QueueCommand(command, false);

                        ASSERT(command->ClientHandlesCommandId(command->command_id));

                        command->client->eventCommand.Signal();
                    }
                }

                _commands_critical.Exit();
            }

            //
            // Iterate through commands, tell attached clients to `Shutdown`.
            //
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

            //
            // // Do all commands have a return status? (Done, Abort, Timeout etc.)
            //
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

        MINTCommand* GetCommandById(CRC command_id);
        bool HandlesCommandId(CRC command_id);

        Win32::Socket* GetSocket()
        {
            return &this->socket;
        }

        Win32::Thread* GetThread()
        {
            return &this->thread;
        }

        // Thread procedure.
        static U32 STDCALL MINTMasterThread(void* context);

        // Client configuration.
        MINTCLIENT::Client::Config config;

    private:
        // Flags
        U32 flags;

        // Client thread.
        Win32::Thread thread;

        // Event to stop the client.
        Win32::EventIndex eventQuit;

        // Event that there's a custom command incoming.
        Win32::EventIndex eventCommand;

        // List of commands that this client currently handles, must be thread safe!
        List<MINTCommand> commands;

        // Process data incoming from server.
        void HandleIncomingPacket(const StyxNet::Packet& packet);

        // Address of the server.
        Win32::Socket::Address address;

        // Socket which connects our client to the server.
        Win32::Socket socket;

        // Event handle for the above socket.
        Win32::EventIndex event;

        // Buffer for receiving data.
        StyxNet::Packet::Buffer* packetBuffer;

        // Index
        U32 index;
    };
}
