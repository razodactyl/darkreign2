#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Identity
    {
        static const unsigned int AuthenticateCommand = 0xFFAADDEE;
        std::wstring username;

        struct Result
        {
            WONAPI::Error error;
        };

        Identity()
        {
        }

        ~Identity()
        {
            //
        }

        std::wstring GetLoginName() { return username; }

        static WONAPI::Error Authenticate(Client* client, const char* username, const char* password, void (*callback)(const Result& result));
    };
}
