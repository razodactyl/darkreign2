#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Identity
    {
        struct Data
        {
            struct Credentials
            {
                StrCrc<32, CH> username;
                StrCrc<32, CH> password;
                StrCrc<32, CH> new_password;
            };
        };

    private:
        Data::Credentials _credentials;
        bool _loggedIn = false;

    public:
        struct Result : CommandResult {};

        Identity() {};

        Identity(const CH* username, const CH* password)
        {
            Set(username, password);
        }

        Data::Credentials Set(const CH* username, const CH* password)
        {
            _credentials.username.Set(username);
            _credentials.password.Set(password);
            return _credentials;
        }

        Data::Credentials SetNewPassword(const CH* new_password)
        {
            _credentials.new_password.Set(new_password);
            return _credentials;
        }

        void SetLoggedIn(bool value)
        {
            _loggedIn = value;
        }

        bool LoggedIn() { return _loggedIn; }
        const CH* GetLoginName() { return _credentials.username.str; }
        const CH* GetPassword() { return _credentials.password.str; }

        static WONAPI::Error Authenticate(Client* client, const CH* username, const CH* password, void (*callback)(const Identity::Result& result), void* context);
        static WONAPI::Error Create(Client* client, const CH* username, const CH* password, void (*callback)(const Identity::Result& result), void* context);
        static WONAPI::Error Update(Client* client, const CH* username, const CH* password, const CH* new_password, void (*callback)(const Identity::Result& result), void* context);

        // Instantiated `Identity` authenticate method called after `Set`.
        WONAPI::Error Authenticate(Client* client, void (*callback)(const Identity::Result& result), void* context)
        {
            return Authenticate(client, _credentials.username.str, _credentials.password.str, callback, context);
        }

        // Instantiated `Identity` authenticate method called after `Set`.
        WONAPI::Error Create(Client* client, void (*callback)(const Identity::Result& result), void* context)
        {
            return Create(client, _credentials.username.str, _credentials.password.str, callback, context);
        }

        // Instantiated `Identity` update method called after `Set`.
        WONAPI::Error Update(Client* client, void (*callback)(const Identity::Result& result), void* context)
        {
            return Update(client, _credentials.username.str, _credentials.password.str, _credentials.new_password.str, callback, context);
        }
    };
}
