#include "pch.h"

#include "RoutingServerClient.h"

#include "Encoding.h"
#include "Identity.h"


namespace MINTCLIENT
{
    U32 STDCALL RoutingServerClient::RoutingServerClientProcess(void* context)
    {
        //
        // MINTCLIENT processed a packet.
        // The MINTCLIENT that processed that packet had a command context sent to it.
        // The MINTCLIENT looked for the command associated with the packet and raised the `commandDone` event.
        //
        //  ContextList->Add->Handles: CommandDone/Abort
        //
        LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " MINTCLIENT Routing Server Client ENTER <<<");

        RoutingServerClient* routing = static_cast<RoutingServerClient*>(context);

        bool done_processing = false;

        while (!done_processing)
        {
            // LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8);<< "]" << " MINTCLIENT Routing Server Client Thread <<<");
            void* event_context;

            if (routing->eventQuit.Wait(0))
            {
                done_processing = true;
                continue;
            }

            // Pass any new commands to the internal client handler.
            routing->command_list->PassToClient();

            if (routing->command_list->GetEvents()->Wait(event_context, FALSE, LoopDependentTimeout))
            {
                auto* cmd = static_cast<Client::MINTCommand*>(event_context);
                LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " MINTCLIENT Routing Server Received Command [" << HEX(cmd->command_id, 8) << "] (" << Message::GetCommandString(cmd->command_id) << ")");
                LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " did_send = " << cmd->did_send << ", did_complete = " << cmd->did_complete << ", did_timeout = " << cmd->did_timeout << ", did_abort = " << cmd->did_abort);

                switch (cmd->command_id)
                {
                    //
                    // Connect first, then register to enter room.
                    //
                case Message::RoutingServerRoomConnect: // 0xEE37226B
                {
                    ConnectRoomResult result;
                    result.error = cmd->GetError();
                    result.context = cmd->context;

                    cmd->callback(result);

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- Connected to room.");

                    // Remove from routing server and client.
                    routing->command_list->Remove(cmd);
                }
                break;

                //
                // Allowed after connection is established.
                //
                case Message::RoutingServerRoomRegister: // 0x59938A8D
                {
                    RegisterClientResult result;
                    result.error = cmd->GetError();
                    result.context = cmd->context;

                    cmd->callback(result);

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- Registered with room.");

                    routing->command_list->Remove(cmd);
                }
                break;

                //
                // User Management.
                //

                case Message::RoutingServerGetUserList: // 0x82E37940
                {
                    GetUserListResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        for (int i = 0; i < tlv->length; i++)
                        {
                            auto clientEntry = Data::ClientData();
                            auto* data = &tlv->items[i];

                            clientEntry.clientId = data->items[0].GetU32();
                            clientEntry.clientName.Set(data->items[1].GetString());
                            clientEntry.isModerator = data->items[3].GetU8();
                            clientEntry.isMuted = data->items[4].GetU8();

                            result.clientDataList.push_back(clientEntry);
                        }

                        delete tlv;
                    }

                    result.context = cmd->context;

                    cmd->callback(result);

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- Get user list.");

                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerGetNumUsers:
                {
                    GetNumUsersResult result;
                    result.error = cmd->GetError();

                    if (result.error == Errors::Success && cmd->client && cmd->data_size > 0)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);
                        result.numUsers = tlv->GetU32();
                        delete tlv;
                    }

                    result.context = cmd->context;
                    cmd->callback(result);

                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerUserEnter: // 0x75BBABEE
                {
                    ClientEnterResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success && cmd->client && cmd->data_size > 0)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);
                        auto* data = &*tlv;

                        result.client.clientId = data->items[0].GetU32();
                        result.client.clientName.Set(data->items[1].GetString());
                        result.client.isModerator = data->items[3].GetU8();
                        result.client.isMuted = data->items[4].GetU8();

                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerUserEnter [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- User entered.");

                    cmd->times_called++;
                }
                break;

                case Message::RoutingServerUserLeave: // 0xCF1E785F
                {
                    ClientLeaveResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success && cmd->client && cmd->data_size > 0)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);
                        auto* data = &*tlv;

                        result.client.clientId = data->items[0].GetU32();

                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerUserLeave [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- User left.");

                    cmd->times_called++;
                }
                break;

                //
                // Room information / messages.
                //

                case Message::RoutingServerBroadcastChat: // 0xC79C5EB4 <<< INBOUND ONLY
                {
                    ASCIIChatMessageResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success && cmd->client && cmd->data_size > 0)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        result.chatMessage.clientId = tlv->items[0].GetU32();
                        result.chatMessage.type = tlv->items[1].GetU8();
                        result.chatMessage.isWhisper = tlv->items[2].GetU8();
                        result.chatMessage.text.Set(tlv->items[3].GetString());

                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerBroadcastChat [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- Broadcast chat.");

                    cmd->times_called++;
                }
                break;

                //
                // Game information / events.
                //

                case Message::RoutingServerCreateGame:
                {
                    CreateGameResult result;
                    result.error = cmd->GetError();
                    cmd->callback(result);
                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerUpdateGame:
                {
                    UpdateGameResult result;
                    result.error = cmd->GetError();
                    cmd->callback(result);
                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerDeleteGame:
                {
                    DeleteGameResult result;
                    result.error = cmd->GetError();
                    cmd->callback(result);
                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerGetGameList:
                {
                    GetGameListResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        for (int i = 0; i < tlv->length; i++)
                        {
                            auto gameEntry = GameResult();
                            auto* data = &tlv->items[i];

                            gameEntry.ownerId = data->items[0].GetU32();
                            gameEntry.name.Set(data->items[1].GetString());

                            auto address = IPSocket::Address(Utils::Unicode2Ansi(data->items[2].GetString()));
                            gameEntry.address = IPSocket::Address(address.host, address.port);

                            auto* data_bytes = data->items[3].GetBytes();

                            // Hack: overwrite game's IP with the address provided by MINT.
                            ((StyxNet::Session*)data_bytes)->address.SetIP(address.host);

                            gameEntry.game_data = data_bytes;
                            gameEntry.game_data_size = data->items[3].length;

                            result.gameResultList.push_back(gameEntry);
                        }

                        delete tlv;
                    }

                    result.context = cmd->context;

                    cmd->callback(result);

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- Get game list.");

                    routing->command_list->Remove(cmd);
                }
                break;

                case Message::RoutingServerGameCreated:
                {
                    CreateGameResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        result.ownerId = tlv->items[0].GetU32();
                        result.name.Set(tlv->items[1].GetString());
                        result.address = IPSocket::Address(Utils::Unicode2Ansi(tlv->items[2].GetString()));

                        auto data_bytes = tlv->items[3].GetBytes();

                        // Hack: overwrite game's IP with the address provided by MINT.
                        ((StyxNet::Session*)data_bytes)->address.SetIP(result.address.host);

                        result.game_data = data_bytes;
                        result.game_data_size = tlv->items[3].length;

                        delete tlv;

                        result.context = cmd->context;
                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- A game was created.");

                    cmd->times_called++;
                }
                break;

                case Message::RoutingServerGameUpdated:
                {
                    UpdateGameResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        result.ownerId = tlv->items[0].GetU32();
                        result.name.Set(tlv->items[1].GetString());
                        result.address = IPSocket::Address(Utils::Unicode2Ansi(tlv->items[2].GetString()));

                        auto data_bytes = tlv->items[3].GetBytes();

                        // Hack: overwrite game's IP with the address provided by MINT.
                        ((StyxNet::Session*)data_bytes)->address.SetIP(result.address.host);

                        result.game_data = data_bytes;
                        result.game_data_size = tlv->items[3].length;

                        delete tlv;

                        result.context = cmd->context;
                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- A game was updated.");

                    cmd->times_called++;
                }
                break;

                case Message::RoutingServerGameReplaced:
                {
                    ReplaceGameResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        result.ownerId = tlv->items[0].GetU32();
                        result.name.Set(tlv->items[1].GetString());
                        result.address = IPSocket::Address(Utils::Unicode2Ansi(tlv->items[2].GetString()));

                        auto data_bytes = tlv->items[3].GetBytes();

                        // Hack: overwrite game's IP with the address provided by MINT.
                        ((StyxNet::Session*)data_bytes)->address.SetIP(result.address.host);

                        result.game_data = data_bytes;
                        result.game_data_size = tlv->items[3].length;

                        delete tlv;

                        result.context = cmd->context;
                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- A game was replaced.");

                    cmd->times_called++;
                }
                break;

                case Message::RoutingServerGameDeleted:
                {
                    DeleteGameResult result;
                    result.error = cmd->GetError();

                    if (result.error == WONAPI::Error_Success)
                    {
                        auto* tlv = new Encoding::TLV(cmd->cmd_data, cmd->data_size);

                        result.ownerId = tlv->items[0].GetU32();
                        result.name.Set(tlv->items[1].GetString());

                        delete tlv;

                        result.context = cmd->context;
                        cmd->callback(result);
                    }

                    LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << " ### Routing Server Client --- A game was deleted.");

                    cmd->times_called++;
                }
                break;
                }
            }
            else
            {
                auto currentCommands = routing->command_list->GetAll();

                for (auto& command : currentCommands)
                {
                    if (command->command_id == Message::RoutingServerGetNumUsers)
                    {
                        if (command->Overtime())
                        {
                            command->DropFromClient();
                            command->Timeout();

                            GetNumUsersResult result;
                            result.error = command->GetError();

                            command->callback(result);

                            routing->command_list->Remove(command);
                        }
                    }
                }
            }
        }

        LDIAG("MINT Routing Server Client - RoutingServerClient::RoutingServerClientProcess [" << HEX(GetCurrentThreadId(), 8) << "]" << "MINTCLIENT Routing Server Client EXIT <<<");
        routing->command_list->ShutdownClients();
        delete routing->command_list;
        return TRUE;
    }

    WONAPI::Error RoutingServerClient::InstallClientEnterCatcher(void (*callback)(const ClientEnterResult& result),
        void* context)
    {
        Client::CommandList* client_enter_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerUserEnter;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        client_enter_command->Add(cmd);

        this->catchers[ID_ClientEnterCatcher].push_back(client_enter_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallClientEnterCatcher [" << HEX(&*client_enter_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallClientLeaveCatcher(void (*callback)(const ClientLeaveResult& result),
        void* context)
    {
        Client::CommandList* client_leave_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerUserLeave;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        client_leave_command->Add(cmd);

        this->catchers[ID_ClientLeaveCatcher].push_back(client_leave_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallClientLeaveCatcher [" << HEX(&*client_leave_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallGameCreatedCatcher(void (*callback)(const CreateGameResult& result),
        void* context)
    {
        Client::CommandList* game_created_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerGameCreated;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        game_created_command->Add(cmd);

        // Push catcher into list for `Register` method to pick up.
        this->catchers[ID_GameCreatedCatcher].push_back(game_created_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallGameCreatedCatcher [" << HEX(&*game_created_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallGameUpdatedCatcher(void (*callback)(const UpdateGameResult& result),
        void* context)
    {
        Client::CommandList* game_updated_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerGameUpdated;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        game_updated_command->Add(cmd);

        // Push catcher into list for `Register` method to pick up.
        this->catchers[ID_GameUpdatedCatcher].push_back(game_updated_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallGameUpdatedCatcher [" << HEX(&*game_updated_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallGameReplacedCatcher(void (*callback)(const ReplaceGameResult& result),
        void* context)
    {
        Client::CommandList* game_replaced_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerGameReplaced;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        game_replaced_command->Add(cmd);

        // Push catcher into list for `Register` method to pick up.
        this->catchers[ID_GameReplacedCatcher].push_back(game_replaced_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallGameReplacedCatcher [" << HEX(&*game_replaced_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallGameDeletedCatcher(void (*callback)(const DeleteGameResult& result),
        void* context)
    {
        Client::CommandList* game_deleted_command = new Client::CommandList();

        auto* cmd = new Client::MINTCommand(nullptr);
        cmd->callback = callback;
        cmd->command_id = Message::RoutingServerGameDeleted;
        cmd->recycle = true;
        cmd->listener_only = true;
        cmd->SetContext(context);

        game_deleted_command->Add(cmd);

        // Push catcher into list for `Register` method to pick up.
        this->catchers[ID_GameDeletedCatcher].push_back(game_deleted_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallGameDeletedCatcher [" << HEX(&*game_deleted_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::InstallASCIIPeerChatCatcher(
        void (*callback)(const ASCIIChatMessageResult& message), void* context)
    {
        Client::CommandList* ascii_chat_command = new Client::CommandList();

        auto* ctx = new Client::MINTCommand(nullptr);
        ctx->callback = callback;
        ctx->command_id = Message::RoutingServerBroadcastChat;  // The command we're wired to handle.
        ctx->recycle = true;                                    // Once processed, allow it to be handled again.
        ctx->listener_only = true;                              // Don't send any data to the server when this command is added to a client's command queue.
        ctx->SetContext(context);                               // The context of the function dealing with the callback.

        ascii_chat_command->Add(ctx);                           // Add this context to the list of contexts.

        // Queue installation for next available connection.
        this->catchers[ID_ASCIIPeerChatCatcher].push_back(ascii_chat_command);

        LDIAG("MINT Routing Server Client - RoutingServerClient::InstallASCIIPeerChatCatcher [" << HEX(&*ascii_chat_command, 8) << "].");

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::GetNumUsers(void (*callback)(const GetNumUsersResult& result), void* context)
    {
        ASSERT(this->client);

        // Create a `MINTCommand` with the command and data to send to the server.
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerGetNumUsers; // 0xACCD008F
        cmd->callback = callback;

        cmd->SetContext(context);
        command_list->Add(cmd);

        command_list->PassToClient(); // Want it queued straight away.

        return WONAPI::Error_Pending;
    }

    //
    // Applicable for a current routing server.
    //
    WONAPI::Error RoutingServerClient::GetUserList(void (*callback)(const GetUserListResult& result), void* context)
    {
        ASSERT(this->client);

        // Create a `MINTCommand` with the command and data to send to the server.
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerGetUserList; // 0x82E37940
        cmd->callback = callback;

        cmd->SetContext(context);
        command_list->Add(cmd);

        return WONAPI::Error_Pending;
    }

    //
    // MINTCLIENT::RoutingServerClient::Connect
    //  - The `RoutingServer` aka Lobby / Chat Server.
    //  - MINTServer is the main Directory Server.
    //  - 1. A client connects to MINTServer and asks for the list of servers / routing servers and authenticates their identity.
    //  - 2. The client has confirmed their identity and uses it subsequently to connect to a specific routing server. (The routing server itself will also verify the identity with an authentication server)
    //  - 3. The player has connected to the server, it has verified their identity and now the player can `Register` with their `username` and the appropriate `password` for this routing server's room.
    //
    WONAPI::Error RoutingServerClient::Connect(IPSocket::Address address, Identity& identity, const CH* room_password,
        bool isReconnect, long timeout,
        void (*callback)(const ConnectRoomResult& result), void* context)
    {
        // Let's instantiate a `MINTCLIENT` and connect it to the specified lobby.
        Client::Config* c = new Client::Config(address);
        Client* client = new Client(*c);

        // We're now connected, other calls will use this `client`.
        this->client = client;

        // Set the command and data to send to the server.
        auto* cmd = new Client::MINTCommand(client);
        cmd->command_id = Message::RoutingServerRoomConnect; // 0xEE37226B
        cmd->callback = callback;

        auto req = ConnectRoomRequest();
        req.username.Set(identity.GetLoginName());
        req.room_password.Set(room_password);
        cmd->SetDataFromStruct(req);

        cmd->SetContext(context);

        command_list->Add(cmd);

        if (!h_RoutingServerClientThread.IsAlive())
        {
            h_RoutingServerClientThread.Start(RoutingServerClientProcess, this);
        }

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::Register(const CH* username, const CH* password, bool becomeHost,
        bool becomeSpec, bool joinChat,
        void (*callback)(const RegisterClientResult& result), void* context)
    {
        ASSERT(this->client);

        // Create a `MINTCommand` with the command and data to send to the server.
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerRoomRegister;
        cmd->callback = callback;

        auto req = RegisterClientRequest();
        req.username.Set(username);
        req.password.Set(password);
        req.becomeHost = becomeHost;
        req.becomeSpec = becomeSpec;
        req.joinChat = joinChat;
        cmd->SetDataFromStruct(req);

        cmd->SetContext(context);

        command_list->Add(cmd);

        for (auto it = this->catchers.begin(); it != this->catchers.end(); ++it)
        {
            // Code isn't written to care about type at the moment, but this is an example of filtering based on catcher type.
            // if (it->first == ID_ASCIIPeerChatCatcher || it->first == ID_ClientEnterCatcher || it->first == ID_ClientLeaveCatcher || it->first == ID_GameCreatedCatcher)
            {
                for (auto command_list_iterator = it->second.begin(); command_list_iterator != it->second.end(); ++
                    command_list_iterator)
                {
                    auto* catcher_command_list = *command_list_iterator;
                    for (auto* catcher_cmd : catcher_command_list->GetAll())
                    {
                        catcher_cmd->client = this->client;
                        command_list->Add(catcher_cmd);
                    }
                }
            }
        }

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::BroadcastChat(std::wstring& text, bool f)
    {
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerBroadcastChat;

        ASCIIChatMessageRequest request;

        request.chatMessage.clientId = this->GetClientId();
        request.chatMessage.type = Data::ASCIIChatMessage::CHATTYPE_ASCII;
        request.chatMessage.text.Set((CH*)text.c_str());
        request.context = nullptr;

        cmd->SetDataFromStruct(request);

        // Bypass client, just send it through the network.
        // Installed catcher will deal with the response from server.
        cmd->AttemptSend();

        return WONAPI::Error_Success;
    }

    WONAPI::Error RoutingServerClient::CreateGame(const CH* name, const U32 clientId, const Client::MINTBuffer& data,
        void (*callback)(const CreateGameResult& result), void* context)
    {
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerCreateGame;
        cmd->callback = callback;

        auto request = CreateGameRequest();
        request.clientId = clientId;
        request.name.Set(name);
        cmd->SetDataFromStruct(request);    // Request data size has to be static for this to work.

        // Game data is dynamic data that we attach to the end of the payload.
        cmd->AddDataBytes(data.data, data.size);

        LDIAG("Routing Server Client -> Create Game:");
        Utils::MemoryDump(cmd->cmd_data, cmd->data_size, 0);

        cmd->SetContext(context);

        command_list->Add(cmd);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::UpdateGame(const CH* name, const U32 clientId, const Client::MINTBuffer& data,
        void (*callback)(const UpdateGameResult& result), void* context)
    {
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerUpdateGame;
        cmd->callback = callback;

        auto request = UpdateGameRequest();
        request.clientId = clientId;
        request.name.Set(name);
        cmd->SetDataFromStruct(request);    // Request data size has to be static for this to work.

        // Game data is dynamic data that we attach to the end of the payload.
        cmd->AddDataBytes(data.data, data.size);

        LDIAG("Routing Server Client -> Update Game:");
        Utils::MemoryDump(cmd->cmd_data, cmd->data_size, 0);

        cmd->SetContext(context);

        command_list->Add(cmd);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::DeleteGame(const CH* name, const U32 clientId,
        void (*callback)(const DeleteGameResult& result), void* context)
    {
        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerDeleteGame;
        cmd->callback = callback;

        auto request = DeleteGameRequest();
        request.clientId = clientId;
        request.name.Set(name);
        cmd->SetDataFromStruct(request);    // Request data size has to be static for this to work.

        cmd->SetContext(context);

        command_list->Add(cmd);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::GetGameList(void (*callback)(const GetGameListResult& result), void* context)
    {
        ASSERT(this->client);

        auto* cmd = new Client::MINTCommand(this->client);
        cmd->command_id = Message::RoutingServerGetGameList; // 0x41D50E29
        cmd->callback = callback;

        cmd->SetContext(context);
        command_list->Add(cmd);

        return WONAPI::Error_Pending;
    }

    U32 RoutingServerClient::GetClientId()
    {
        ASSERT(this->client);
        return 0x00000000;
    }

    Win32::Socket* RoutingServerClient::GetSocket()
    {
        ASSERT(this->client);
        return this->client->GetSocket();
    }

    void RoutingServerClient::Close()
    {
        this->catchers[ID_ASCIIPeerChatCatcher].clear();
        this->client->Shutdown();
        this->eventQuit.Signal();
    }
}
