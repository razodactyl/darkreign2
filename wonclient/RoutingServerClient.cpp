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
                case MINTCLIENT::RoutingServerClient::ConnectRoomCommand:
                {
                    RoutingServerClient::ConnectRoomResult result = *(RoutingServerClient::ConnectRoomResult*)cc->data;
                    result.error = WONAPI::Error_Success;
                    contextList->callback(result);
                    //delete cc;
                }
                break;

                case MINTCLIENT::RoutingServerClient::Room_RegisterCommand:
                {
                    RoutingServerClient::RegisterClientResult result = *(RoutingServerClient::RegisterClientResult*)cc->data;
                    result.error = WONAPI::Error_Success;
                    contextList->callback(result);
                }
                break;
                }

                doneProcessing = true;
            }
        }

        LDIAG("MINTCLIENT routingServerClient thread exit.");
        return TRUE;
    }

    WONAPI::Error RoutingServerClient::GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result))
    {
        return WONAPI::Error_Pending;
    }

    WONAPI::Error RoutingServerClient::Register(const char* loginName, const char* password, bool becomeHost, bool becomeSpectator, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result))
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::RegisterClientResult>();
        contextList->callback = callback;

        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(this->client);
        ctx->command = MINTCLIENT::RoutingServerClient::Room_RegisterCommand;

        auto registerClientResult = RegisterClientResult();

        ctx->SetData(registerClientResult);
        contextList->Add(ctx);
        this->client->SendCommand(ctx);

        // Last call should have finished by now.
        ASSERT(!routingServerClientThread.IsAlive());

        // Go away, process the command, callback when we have a response.
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    //
    // MINTCLIENT::RoutingServerClient::Connect
    //
    WONAPI::Error RoutingServerClient::Connect(MINTCLIENT::IPSocket::Address routingAddress, MINTCLIENT::Identity* identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result))
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::ConnectRoomResult>();
        contextList->callback = callback;

        MINTCLIENT::Client::Config* c = new MINTCLIENT::Client::Config(Win32::Socket::Address(routingAddress.ip, U16(routingAddress.port)));
        MINTCLIENT::Client* client = new MINTCLIENT::Client(*c);
        this->client = client;

        // Create a `CommandContext` with the command and data to send to the server.
        auto* ctx = new Client::CommandContext(client);
        ctx->command = MINTCLIENT::RoutingServerClient::ConnectRoomCommand;

        auto connectRoomContext = ConnectRoomResult();//Data::ConnectRoomContext();
        connectRoomContext.context.roomName.Set("");
        memset(connectRoomContext.context.roomName.str, 'N', 32);

        connectRoomContext.context.password.Set("");
        memset(connectRoomContext.context.password.str, 'P', 32);

        connectRoomContext.context.routingServer = this;
        connectRoomContext.error = WONAPI::Error_Success;
        connectRoomContext.message.Set("");
        memset(connectRoomContext.message.str, '#', 256);
        ctx->SetData(connectRoomContext);

        contextList->Add(ctx);
        client->SendCommand(ctx);

        // Last call should have finished by now.
        ASSERT(!routingServerClientThread.IsAlive());

        // Go away, process the command, callback when we have a response.
        routingServerClientThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }

    void RoutingServerClient::Close()
    {
        //
    }
}
