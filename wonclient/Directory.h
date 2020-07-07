#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


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
    };
}
