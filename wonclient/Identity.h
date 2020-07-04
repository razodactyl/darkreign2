#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Identity
    {
        struct Data
        {
            struct Identity
            {
                StrCrc<32, CH> username;
                StrCrc<32, CH> password;
            };
        };

        struct Result : CommandResult
        {
        };

        Identity() {};

        Data::Identity _identity;
        Identity(const CH* username, const CH* password)
        {
            Set(username, password);
        }

        Data::Identity Set(const CH* username, const CH* password)
        {
            _identity.username.Set(username);
            _identity.password.Set(password);
            return _identity;
        }

        CH* GetLoginName()
        {
            return _identity.username.str;
        }

        static WONAPI::Error Authenticate(Client* client, const CH* username, const CH* password, void (*callback)(const Identity::Result& result), void* context);
        static WONAPI::Error Create(Client* client, const CH* username, const CH* password, void (*callback)(const Identity::Result& result), void* context);

        // Instantiated `Identity` authenticate method called after `Set`.
        WONAPI::Error Authenticate(Client* client, void (*callback)(const Identity::Result& result), void* context)
        {
            return Authenticate(client, _identity.username.str, _identity.password.str, callback, context);
        }

        // Instantiated `Identity` authenticate method called after `Set`.
        WONAPI::Error Create(Client* client, void (*callback)(const Identity::Result& result), void* context)
        {
            return Create(client, _identity.username.str, _identity.password.str, callback, context);
        }
    };
}
