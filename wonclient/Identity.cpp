#include "pch.h"

#include "Identity.h"
#include "MINTCLIENT.h"


namespace MINTCLIENT
{
    Win32::Thread identityThread;
    U32 STDCALL Process(void* context)
    {
        auto cl = static_cast<Client::ContextList<MINTCLIENT::Identity::Result>*>(context);

        bool doneProcessing = false;

        while (!doneProcessing)
        {
            void* ctx;
            if (cl->events.Wait(ctx, TRUE))
            {
                const auto cc = static_cast<Client::CommandContext*>(ctx);
                switch (cc->command)
                {
                case MINTCLIENT::Identity::AuthenticateCommand:
                    Identity::Result result;

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
    // MINT::Identity::Authenticate
    //
    WONAPI::Error Identity::Authenticate(MINTCLIENT::Client* client, const char* username, const char* password, void (*callback)(const MINTCLIENT::Identity::Result& result))
    {
        auto* cl = new MINTCLIENT::Client::ContextList<MINTCLIENT::Identity::Result>();
        cl->callback = callback;

        // Go away, process the command, tell me when there's a result.
        auto ctx = new Client::CommandContext(client);
        ctx->command = MINTCLIENT::Identity::AuthenticateCommand;
        cl->Add(ctx);

        client->SendCommand(ctx);

        // Should have finished by now.
        ASSERT(!identityThread.IsAlive());

        identityThread.Start(Process, cl);

        return WONAPI::Error_Pending;
    }
}
