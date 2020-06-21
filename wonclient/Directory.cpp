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
                    case MINTCLIENT::Message::DirectoryListServers:
                    {
                        MINTCLIENT::Directory::Result result;

                        auto directoryEntity = MINTCLIENT::Directory::Data::DirectoryEntity();
                        directoryEntity.name.Set((CH*)L"AuthServer");
                        directoryEntity.address.Set((CH*)"192.168.0.106:15101");
                        result.entityList.push_back(directoryEntity);

                        auto directoryEntity2 = MINTCLIENT::Directory::Data::DirectoryEntity();
                        directoryEntity2.name.Set((CH*)L"TitanRoutingServer");
                        directoryEntity2.address.Set((CH*)"192.168.0.106:15101");
                        result.entityList.push_back(directoryEntity2);

                        // Pass context to calling function.
                        result.context = cc->context;

                        result.error = WONAPI::Error_Success;
                        contextList->callback(result);
                        break;
                    }

                    case MINTCLIENT::Message::DirectoryListRooms:
                    {
                        MINTCLIENT::Directory::Result result;

                        auto directoryEntity = MINTCLIENT::Directory::Data::DirectoryEntity();
                        directoryEntity.name.Set((CH*)L"TitanRoutingServer");
                        result.entityList.push_back(directoryEntity);

                        auto directoryEntity2 = MINTCLIENT::Directory::Data::DirectoryEntity();
                        directoryEntity2.name.Set((CH*)L"TitanRoutingServer");
                        result.entityList.push_back(directoryEntity2);

                        // Pass context to calling function.
                        result.context = cc->context;

                        result.error = WONAPI::Error_Success;
                        contextList->callback(result);
                        break;
                    }
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
        void (*callback)(const MINTCLIENT::Directory::Result& result),
        void* context
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
                ctx->command = MINTCLIENT::Message::DirectoryListRooms;
                ctx->SetData(identity);
            }
            else
            {
                ctx->command = MINTCLIENT::Message::DirectoryListServers;
            }

            ctx->SetContext(context);

            contextList->Add(ctx);

            client->SendCommand(ctx);
        }

        ASSERT(!directoryThread.IsAlive());

        directoryThread.Start(Directory::Process, contextList);

        return WONAPI::Error_Pending;
    }
}
