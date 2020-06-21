#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Directory
    {
        struct Data
        {
            struct DirectoryEntity
            {
                StrCrc<32, CH> name;
                StrCrc<32, CH> address;
                bool permanent = true;
            };

            typedef std::list<DirectoryEntity> DirectoryEntityList;
        };

        struct Result : CommandResult
        {
            Data::DirectoryEntityList entityList;
        };

        static U32 STDCALL Process(void* context);

        static WONAPI::Error GetDirectory(MINTCLIENT::Identity* identity, const std::vector<MINTCLIENT::IPSocket::Address>* servers, void (*callback)(const Result& result), void* context);
    };
}
