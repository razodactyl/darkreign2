#include "pch.h"

#include "RoutingServerClient.h"


namespace MINTCLIENT
{
    Win32::Thread routingServerClientThread;
    U32 STDCALL RoutingServerClient::Process(void* context)
    {
        LDIAG("MINTCLIENT routingServerClient thread enter.");

        auto contextList = static_cast<Client::ContextList<MINTCLIENT::RoutingServerClient::Result>*>(context);

        bool doneProcessing = false;

        while (!doneProcessing)
        {
            void* ctx;
            if (contextList->events.Wait(ctx, TRUE))
            {
                const auto cc = static_cast<Client::CommandContext*>(ctx);
                switch (cc->command)
                {
                    //
                    // Connect first, then register to enter room.
                    //
                    case MINTCLIENT::Message::RoutingServerRoomConnect:
                    {
                        RoutingServerClient::ConnectRoomResult result;// = *reinterpret_cast<RoutingServerClient::ConnectRoomResult*>(cc->data);
                        result.error = WONAPI::Error_Success;
                        result.context = cc->context;
                        contextList->callback(result);
                    }
                    break;

                    //
                    // Allowed after connection is established.
                    //
                    case MINTCLIENT::Message::RoutingServerRoomRegister:
                    {
                        RoutingServerClient::RegisterClientResult result;// = *reinterpret_cast<RoutingServerClient::RegisterClientResult*>(cc->data);
                        result.error = WONAPI::Error_Success;
                        result.context = cc->context;
                        contextList->callback(result);
                    }
                    break;

                    case MINTCLIENT::Message::RoutingServerGetUserList:
                    {
                        RoutingServerClient::GetClientListResult result;

                        auto cd1 = MINTCLIENT::RoutingServerClient::Data::ClientData();
                        cd1.clientName.Set((CH*)L"Hello");
                        result.clientDataList.push_back(cd1);

                        result.error = WONAPI::Error_Success;
                        result.context = cc->context;
                        contextList->callback(result);
                    }
                    break;

                    case MINTCLIENT::Message::RoutingServerBroadcastChat:
                    {
                        auto* result = new RoutingServerClient::ASCIIChatMessageResult(); // What about Unicode? (Might need an alternate handler)
                        const auto d = reinterpret_cast<RoutingServerClient::ASCIIChatMessageResult*>(cc->cmd_data); // What about Unicode? (Might need an alternate handler)

                        result->chatMessage = Data::ASCIIChatMessage();
                        result->chatMessage.mData.assign(d->chatMessage.mData);
                        result->chatMessage.mChatType = d->chatMessage.mChatType; // Data::ASCIIChatMessage::CHATTYPE_ASCII | Data::UnicodeChatMessage::CHATTYPE_UNICODE
                        result->chatMessage.mSenderId = 0;//  d->chatMessage.mSenderId; // 0x1122

                        contextList->callback(*result);
                        continue;
                    }
                    break;
                }

                // Don't forget to delete objects allocated with `new`
                // delete cc;
                doneProcessing = true;
            }
        }

        LDIAG("MINTCLIENT routingServerClient thread exit.");
        return TRUE;
    }

    WONAPI::Error RoutingServerClient::InstallASCIIPeerChatCatcher(void (*callback)(const RoutingServerClient::ASCIIChatMessageResult& message), void* context)
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::ASCIIChatMessageResult>();
        contextList->callback = callback;
        
        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(this->client);
        ctx->command = MINTCLIENT::Message::RoutingServerBroadcastChat;
        ctx->SetContext(context);
        contextList->Add(ctx);

        // Can't send it off until `client` instantiated via `Connect` etc.
        if (this->client)
        {
            this->client->SendCommand(ctx);
        }
        else
        {
            // Queue installation for later.
            this->catchers[0].push_back(contextList);
        }
        
        // ASSERT(!routingServerClientThread.IsAlive());
        if (routingServerClientThread.IsAlive())
        {
            LOG_DEV(("!!! An existing `RoutingServerClient` thread is already running!!! (InstallASCIIPeerChatCatcher)"));
        }
        
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result), void* context)
    {
        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::GetUserList(void (*callback)(const RoutingServerClient::GetClientListResult& result), void* context)
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::GetClientListResult>();
        contextList->callback = callback;

        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(this->client);
        ctx->command = MINTCLIENT::Message::RoutingServerGetUserList;

        auto getClientListResult = GetClientListResult();

        //ctx->SetData(getClientListResult);
        ctx->SetContext(context);
        contextList->Add(ctx);
        this->client->SendCommand(ctx);

        // Last call should have finished by now.
        // ASSERT(!routingServerClientThread.IsAlive());
        if (routingServerClientThread.IsAlive())
        {
            LOG_DEV(("!!! An existing `RoutingServerClient` thread is already running!!! (GetUserList)"));
        }

        // Go away, process the command, callback when we have a response.
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::Register(const CH* loginName, const CH* password, bool becomeHost, bool becomeSpec, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result), void* context)
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::RegisterClientResult>();
        contextList->callback = callback;

        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(this->client);
        ctx->command = MINTCLIENT::Message::RoutingServerRoomRegister;

        // Package request for server to unpack.
        auto req = RegisterClientRequest();
        req.username.Set(loginName);
        req.password.Set(password);
        req.becomeHost = becomeHost;
        req.becomeSpec = becomeSpec;
        req.joinChat = joinChat;
        ctx->SetData(req);

        ctx->SetContext(context);

        // Package list of context information for MINTCLIENT
        contextList->Add(ctx);
        // Notify MINTCLIENT of new command.
        this->client->SendCommand(ctx);

        // Last call should have finished by now.
        // ASSERT(!routingServerClientThread.IsAlive());
        if (routingServerClientThread.IsAlive())
        {
            LOG_DEV(("!!! An existing `RoutingServerClient` thread is already running!!! (Register)"));
        }

        // Go away, process the command, callback when we have a response.
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    //
    // MINTCLIENT::RoutingServerClient::Connect
    // - A routing server is a specific server hosting a chat room / lobby.
    // When we request the list of servers from the main directory, it
    // returns one of these servers with an address.
    // - Note that there's a distinction between the main server and one of these lobbies.
    //
    WONAPI::Error RoutingServerClient::Connect(MINTCLIENT::IPSocket::Address routingAddress, MINTCLIENT::Identity* identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result), void* context)
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::ConnectRoomResult>();
        contextList->callback = callback;

        // Let's instantiate a `MINTCLIENT` and connect it to the specified lobby.
        MINTCLIENT::Client::Config* c = new MINTCLIENT::Client::Config(Win32::Socket::Address(routingAddress.ip, U16(routingAddress.port)));
        MINTCLIENT::Client* client = new MINTCLIENT::Client(*c);
        // We're now dealing with this specific client / connection.
        this->client = client;

        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(client);
        ctx->command = MINTCLIENT::Message::RoutingServerRoomConnect;

        // At this point, we're just trying to `Connect` at all.
        auto req = RoutingServerClient::ConnectRoomRequest();

        // auto connectRoomContext = ConnectRoomResult();
        // connectRoomContext.context.roomName.Set("");
        // memset(connectRoomContext.context.roomName.str, 'N', 32);
        // connectRoomContext.context.password.Set("");
        // memset(connectRoomContext.context.password.str, 'P', 32);
        // connectRoomContext.context.routingServer = this;
        // connectRoomContext.error = WONAPI::Error_Success;
        // connectRoomContext.message.Set("");
        // memset(connectRoomContext.message.str, '#', 256);
        // ctx->SetData(connectRoomContext);

        for (auto c : this->catchers[0])
        {
            c->contexts.back().client = client;

            // for (auto cc : c->contexts)
            // {
            //     //cc.client = client;
            //     //contextList->Add(&cc);
            // }
        }

        ctx->SetContext(context);
        contextList->Add(ctx);
        client->SendCommand(ctx);

        // Last call should have finished by now.
        // ASSERT(!routingServerClientThread.IsAlive());
        if (routingServerClientThread.IsAlive())
        {
            LOG_DEV(("!!! An existing `RoutingServerClient` thread is already running!!! (Connect)"));
        }

        // Go away, process the command, callback when we have a response.
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::BroadcastChat(std::wstring& text, bool f) const
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::Result>();

        auto* ctx = new Client::CommandContext(client);
        ctx->command = MINTCLIENT::Message::RoutingServerBroadcastChat;

        ASCIIChatMessageResult result;
        result.chatMessage.mChatType = Data::ASCIIChatMessage::CHATTYPE_ASCII;
        result.chatMessage.mData.assign(Utils::Unicode2Ansi((CH*)text.c_str()));
        result.message.Set("The `result.message` goes here.");
        result.error = WONAPI::Error_Success;

        ctx->SetData(result);

        contextList->Add(ctx);
        client->SendCommand(ctx);

        // Installed `Catcher` is responsible for interception and reporting of these events.
        // Our job is done at this point.

        return WONAPI::Error_Success;
    }

    void RoutingServerClient::Close()
    {
        // `RoutingServerClient` isn't responsible for deleting `MINTCLIENT`.
        this->client->Shutdown();

        // We are however responsible for closing our own thread.
        // TODO JONATHAN
    }
}
