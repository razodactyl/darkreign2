#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Identity
    {
        static const unsigned int AuthenticateCommand = 0xFFAADDEE;

        struct Data
        {
            struct Identity
            {
                StrCrc<32> username;
                StrCrc<32> password;
            };
        };

        struct Result : CommandResult
        {
        };

        Identity() {};

        Data::Identity _identity;
        Identity(const char* username, const char* password)
        {
            Set(username, password);
        }

        Data::Identity Set(const char* username, const char* password)
        {
            _identity.username.Set(username);
            _identity.password.Set(password);
            return _identity;
        }

        char* GetLoginName()
        {
            return _identity.username.str;
        }

        // Class `Identity` authenticate method.
        static WONAPI::Error Authenticate(Client* client, const char* username, const char* password, void (*callback)(const Identity::Result& result));

        // Instantiated `Identity` authenticate method.
        WONAPI::Error Authenticate(Client* client, void (*callback)(const Identity::Result& result))
        {
            return Authenticate(client, _identity.username.str, _identity.password.str, callback);
        }
    };
}
