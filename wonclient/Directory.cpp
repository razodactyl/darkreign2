#include "pch.h"

#include "Directory.h"

#include "Encoding.h"
#include "MINTCLIENT.h"


namespace MINTCLIENT
{
    Win32::Thread Directory::downloadThread;
    U32 STDCALL Directory::DownloadProcessor(void* context)
    {
        auto *ctx = (DownloadContext<void*>*)context;

        httplib::Client cli(ctx->hostname, ctx->port);

        auto p = MINTCLIENT::IPSocket::Address(ctx->proxy);

        // cli.set_proxy(p.GetHost(), p.GetPortI());

        // struct HTTPData
        // {
        //     U32 handle;
        //     bool isNew;
        //     time_t time;
        //     Bool abort;
        //     void* data;
        // };

        // cli.set_follow_location(true);

        std::string body;
        
        auto file = File();
        file.Open(ctx->saveAsPath, File::CREATE | File::WRITE);
        
        std::shared_ptr<httplib::Response> res = cli.Get(
            ctx->getPath, httplib::Headers(),
            [&](const httplib::Response& response) {
                if (response.status != 200)
                {
                    file.Close();
                    ctx->getCallback(MINTCLIENT::Errors::GeneralFailure, ctx->data);
                    return false;
                }
                return true; // return 'false' if you want to cancel the request.
            },
            [&](const char* data, size_t data_length) {
                file.Write(data, data_length);
                body.append(data, data_length);
                return true; // return 'false' if you want to cancel the request.
            },
            [&](uint64_t len, uint64_t total)
            {
                bool result = ctx->progressCallback(len, total, ctx->data);
        
                if (len >= total)
                {
                    file.Close();
                    ctx->getCallback(MINTCLIENT::Errors::Success, ctx->data);
                }
        
                return result; // return 'false' if you want to cancel the request.
            }
        );

        return 0;
    }

    Win32::Thread directoryThread;
    U32 STDCALL Directory::ProcessDirectoryMessage(void* context)
    {
        auto* command_list = static_cast<Client::CommandList*>(context);

        bool done_processing = false;

        while (!done_processing)
        {
            command_list->PassToClient();

            void* ctx;
            auto current_events = command_list->GetEvents();
            // Note: `Wait` will reset any signal once it has been read.
            if (current_events->Wait(ctx, FALSE, MINTCLIENT::DefaultTimeout))
            {
                Client::MINTCommand* cc = (Client::MINTCommand*)(ctx);

                switch (cc->command_id)
                {
                    case MINTCLIENT::Message::DirectoryListServers: // 0x77712BCE
                    {
                        //
                        // Server returns MINTCLIENT::Message::DirectoryListServers (0x77712BCE)
                        // The `cmd_data` appears in the form of a TLV array.
                        // - Each entry of the array is provided in the order of:
                        //  - server.type -> TLVString          e.g "AuthServer"
                        //  - server.name -> TLVString          e.g "<MINTCLIENT Authorisation Server>"
                        //  - server.address -> TLVString       e.g "192.168.0.106:15101"
                        //  - server.permanent -> TLVUint8      e.g 0x01
                        //
                        MINTCLIENT::Directory::Result result;

                        result.error = cc->GetError();

                        if (result.error == WONAPI::Error_Success) {
                            auto* tlv = new MINTCLIENT::Encoding::TLV(cc->cmd_data, cc->data_size);

                            for (U32 i = 0; i < tlv->length; i++)
                            {
                                auto server = MINTCLIENT::Directory::Data::DirectoryServer();

                                auto* data = &tlv->items[i];

                                server.type.Set(data->items[0].GetString());
                                server.name.Set(data->items[1].GetString());
                                server.address.Set(data->items[2].GetString());
                                server.permanent = data->items[3].GetU8Bool();

                                result.server_list.push_back(server);
                            }

                            delete tlv;
                        }

                        result.context = cc->context;
                        cc->callback(result);
                    }
                    break;

                    case MINTCLIENT::Message::DirectoryListRooms: // 0x910DB9D4
                    {
                        MINTCLIENT::Directory::Result result;
                        result.error = cc->GetError();

                        if (result.error == WONAPI::Error_Success) {

                            auto* tlv = new MINTCLIENT::Encoding::TLV(cc->cmd_data, cc->data_size);

                            for (U32 i = 0; i < tlv->length; i++)
                            {
                                auto server = MINTCLIENT::Directory::Data::RoutingServer();

                                auto* data = &tlv->items[i];

                                server.type.Set(data->items[0].GetString());
                                server.name.Set(data->items[1].GetString());
                                server.address.Set(data->items[2].GetString());
                                server.permanent = data->items[3].GetU8Bool();

                                server.password = data->items[4].GetU8Bool();
                                server.num_players = data->items[5].GetU8();

                                result.room_list.push_back(server);
                            }

                            delete tlv;
                        }

                        result.context = cc->context;
                        cc->callback(result);
                    }
                    break;
                }

                done_processing = true;
            }
            else
            {
                command_list->TimeoutAll();
            }
        }

        command_list->ShutdownClients();
        delete command_list;
        return TRUE;
    }

    //
    // MINTCLIENT::Directory::GetDirectory
    //  - Returns list of servers.
    //  - Returns list of rooms.
    //
    WONAPI::Error Directory::GetDirectory(
        MINTCLIENT::Identity* identity,
        const std::vector<MINTCLIENT::IPSocket::Address>* servers,
        void (*callback)(const MINTCLIENT::Directory::Result& result),
        void* context
    )
    {
        auto* command_list = new MINTCLIENT::Client::CommandList();

        for (auto server = servers->begin(); server != servers->end(); ++server)
        {
            MINTCLIENT::Client::Config* c = new MINTCLIENT::Client::Config(server->text);
            MINTCLIENT::Client* client = new MINTCLIENT::Client(*c);

            auto* cmd = new Client::MINTCommand(client);

            cmd->callback = callback;

            if (identity)
            {
                cmd->command_id = MINTCLIENT::Message::DirectoryListRooms;
                cmd->SetDataFromStruct(identity);
            }
            else
            {
                cmd->command_id = MINTCLIENT::Message::DirectoryListServers;
            }

            cmd->SetContext(context);

            command_list->Add(cmd);
        }

        if (!directoryThread.IsAlive()) {
            directoryThread.Start(ProcessDirectoryMessage, command_list);
        }

        return WONAPI::Error_Pending;
    }
}
