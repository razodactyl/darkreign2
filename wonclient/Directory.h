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
                bool permanent = true;
            };

            typedef std::list<DirectoryServer> DirectoryServerList;
        };

        struct Result : CommandResult
        {
            Data::DirectoryServerList server_list;
        };

        static U32 STDCALL ProcessDirectoryMessage(void* context);

        static WONAPI::Error GetDirectory(MINTCLIENT::Identity* identity, const std::vector<MINTCLIENT::IPSocket::Address>* servers, void (*callback)(const Result& result), void* context);
    };
}
