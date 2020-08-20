#pragma once

#include "MINTCLIENT.h"
#include "Errors.h"

#include "httplib.h"
#include "file.h"


namespace MINTCLIENT
{
    struct Directory
    {
        struct Data
        {
            struct DirectoryServer
            {
                StrCrc<32, CH> type;
                StrCrc<32, CH> name;
                StrCrc<32, CH> address;

                bool permanent = false;
            };

            struct RoutingServer : DirectoryServer
            {
                U8 num_players = -1;
                bool password = false;
            };

            typedef std::list<DirectoryServer> DirectoryServerList;
            typedef std::list<RoutingServer> RoutingServerList;
        };

        struct Result : CommandResult
        {
            Data::DirectoryServerList server_list;
            Data::RoutingServerList room_list;
        };

        static U32 STDCALL ProcessDirectoryMessage(void* context);

        static WONAPI::Error GetDirectory(MINTCLIENT::Identity* identity, const std::vector<MINTCLIENT::IPSocket::Address>* servers, void (*callback)(const Result& result), void* context);

        // // //

        template <typename T> struct DownloadContext
        {
            bool (*progressCallback)(unsigned long, unsigned long, void*);
            void (*getCallback)(unsigned int, T);
            void* data;

            char* proxy;
            char* hostname;
            unsigned short port = 0;
            char* getPath;
            char* saveAsPath;
        };

        static Win32::Thread downloadThread;
        static U32 STDCALL DownloadProcessor(void* context);

        // HTTPGet: 45.76.120.39:8000 dr2.mintsoft.dev 8000 /motd/darkreign2/downloads/motd.cfg downloads/motd.cfg 0
        // https://github.com/yhirose/cpp-httplib

        template <typename T> static int HTTPGet(
            const char* proxy, 
            const char* hostname, 
            const unsigned short port, 
            const char* getPath,
            const char* saveAsPath,

            bool progressCallback(unsigned long progress, unsigned long size, void* ctx), 
            void getCallback(unsigned int error, T ctx),
            void* context
        )
        {
            DownloadContext<T>* d = new DownloadContext<T>();
            d->progressCallback = progressCallback;
            d->getCallback = getCallback;
            d->data = context;

            d->proxy = Utils::Strdup(proxy);
            d->hostname = Utils::Strdup(hostname);
            d->port = port;
            d->getPath = Utils::Strdup(getPath);
            d->saveAsPath = Utils::Strdup(saveAsPath);

            downloadThread.Start(DownloadProcessor, d);

            return 0;
        }
    };
}
