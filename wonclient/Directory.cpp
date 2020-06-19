#include "pch.h"

#include "Directory.h"


namespace MINTCLIENT
{
    Win32::Thread directoryThread;
    U32 STDCALL Directory::Process(void* context)
    {
        LDIAG("MINTCLIENT directory thread enter.");

        auto contextList = static_cast<Client::ContextList<MINTCLIENT::Directory::Result>*>(context);

        bool doneProcessing = false;

        while (!doneProcessing)
        {
            void* ctx;
            if (contextList->events.Wait(ctx, TRUE))
            {
                const auto cc = static_cast<Client::CommandContext*>(ctx);
                switch (cc->command)
                {
                case MINTCLIENT::Directory::DirectoryCommand:
                    MINTCLIENT::Directory::Result result;

                    auto directoryEntity = MINTCLIENT::Directory::Data::DirectoryEntity();
                    directoryEntity.mName = L"AuthServer";
                    result.entityList.push_back(directoryEntity);

                    auto directoryEntity2 = MINTCLIENT::Directory::Data::DirectoryEntity();
                    directoryEntity.mName = L"TitanRoutingServer";
                    result.entityList.push_back(directoryEntity);

                    result.error = WONAPI::Error_Success;
                    contextList->callback(result);
                    break;
                }

                doneProcessing = true;
            }
        }

        LDIAG("MINTCLIENT directory thread exit.");
        return TRUE;
    }

    //
    // MINT:Router:GetDirectoryEx
    //
    WONAPI::Error Directory::GetDirectory(
        MINTCLIENT::Identity* identity,
        const std::vector<MINTCLIENT::IPSocket::Address>* servers,
        void (*callback)(const MINTCLIENT::Directory::Result& result)
    )
    {
        auto* contextList = new MINTCLIENT::Client::ContextList<MINTCLIENT::Directory::Result>();
        contextList->callback = callback;

        for (auto server = servers->begin(); server != servers->end(); ++server)
        {
            MINTCLIENT::Client::Config* c = new MINTCLIENT::Client::Config(Win32::Socket::Address(server->ip, U16(server->port)));
            MINTCLIENT::Client* client = new MINTCLIENT::Client(*c);

            auto* ctx = new Client::CommandContext(client);

            if (identity)
            {
                ctx->SetData(identity);
            }
            ctx->command = MINTCLIENT::Directory::DirectoryCommand;
            contextList->Add(ctx);

            client->SendCommand(ctx);
        }

        ASSERT(!directoryThread.IsAlive());

        directoryThread.Start(Directory::Process, contextList);

        return WONAPI::Error_Pending;
    }
}
