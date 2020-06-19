#include "pch.h"

#include "Identity.h"


namespace MINTCLIENT
{
    Win32::Thread identityThread;
    U32 STDCALL Process(void* context)
    {
        LDIAG("MINTCLIENT identityThread thread enter");

        auto contextList = static_cast<Client::ContextList<MINTCLIENT::Identity::Result>*>(context);

        bool doneProcessing = false;

        while (!doneProcessing)
        {
            void* ctx;
            if (contextList->events.Wait(ctx, TRUE))
            {
                const auto cc = static_cast<Client::CommandContext*>(ctx);
                switch (cc->command)
                {
                case MINTCLIENT::Identity::AuthenticateCommand:
                    Identity::Result result = *(Identity::Result*)cc->data;
                    result.error = WONAPI::Error_Success;
                    contextList->callback(result);
                    delete cc;

                    break;
                }

                doneProcessing = true;
            }
        }

        LDIAG("MINTCLIENT::identityThread exit");
        return TRUE;
    }


    //
    // MINT::Identity::Authenticate
    //
    WONAPI::Error Identity::Authenticate(MINTCLIENT::Client* client, const char* username, const char* password, void (*callback)(const Identity::Result& result))
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::Identity::Result>();
        contextList->callback = callback;

        // Create a `CommandContext` with the command and data to send to the server.
        auto *ctx = new Client::CommandContext(client);
        ctx->command = MINTCLIENT::Identity::AuthenticateCommand;

        auto identity = Data::Identity();
        identity.username.Set(username);
        identity.password.Set(password);
        ctx->SetData(identity);

        contextList->Add(ctx);
        client->SendCommand(ctx);

        // Last call should have finished by now.
        ASSERT(!identityThread.IsAlive());

        // Go away, process the command, callback when we have a response.
        identityThread.Start(Process, contextList);

        return WONAPI::Error_Pending;
    }
}
