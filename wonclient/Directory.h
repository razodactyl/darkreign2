#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Directory
    {
        static const unsigned int DirectoryCommand = 0x11223344;

        struct Data
        {
            struct DirectoryEntity
            {
                std::wstring mName;
                bool permanent = true;
            };

            typedef std::list<DirectoryEntity> DirectoryEntityList;
        };

        struct Result : CommandResult
        {
            Data::DirectoryEntityList entityList;
        };

        static U32 STDCALL Process(void* context);
        static WONAPI::Error GetDirectory(MINTCLIENT::Identity* identity, const std::vector<MINTCLIENT::IPSocket::Address>* servers, void (*callback)(const Result& result));
    };
}
