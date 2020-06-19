#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct RoutingServerClient
    {
        static const unsigned int GetNumUsersCommand = 0x00000000;

        static const unsigned int CreateGameCommand = 0x00000001;
        static const unsigned int UpdateGameCommand = 0x00000002;
        static const unsigned int DeleteGameCommand = 0x00000003;

        static const unsigned int GetUserAddressCommand = 0x00000004;

        static const unsigned int BroadcastChatCommand = 0x00000005;
        static const unsigned int WhisperChatCommand = 0x00000006;

        static const unsigned int ConnectRoomCommand = 0x00000007;
        static const unsigned int Room_RegisterCommand = 0x00000009;
        static const unsigned int DisconnectCommand = 0x00000008;

        struct Data
        {
            struct ConnectRoomContext
            {
                StrCrc<32> roomName;
                StrCrc<32> password;
                MINTCLIENT::RoutingServerClient* routingServer;
            };

            struct GetClientListResult
            {
                //
            };

            struct ReadDataObjectResult
            {
                //
            };

            struct GetNumUsersResult
            {
                //
            };

            struct ClientDataWithReason
            {
                //
            };

            struct ClientIdWithReason
            {
                //
            };

            struct ClientIdWithFlag
            {
                //
            };

            struct ClientId
            {
                //
            };

            struct DataObjectWithLifespan
            {
                //
            };

            struct DataObject
            {
                //
            };

            struct DataObjectModification
            {
                //
            };

            struct ASCIIChatMessage
            {
                //
            };

            struct UnicodeChatMessage
            {
                //
            };

            struct RawChatMessage
            {
                //
            };
        };

        struct Result : CommandResult
        {
        };

        struct ConnectRoomResult : Result
        {
            RoutingServerClient::Data::ConnectRoomContext context;
        };

        struct RegisterClientResult : Result
        {
        };

        struct GetNumUsersResult : Result
        {
            //
        };

        // Persistent connection to server.
        MINTCLIENT::Client* client;

        static WONAPI::Error InstallClientEnterCatcher();

        WONAPI::Error GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result));

        WONAPI::Error Register(const char* loginName, const char* password, bool becomeHost, bool becomeSpectator, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result));

        WONAPI::Error Connect(MINTCLIENT::IPSocket::Address routingAddress, MINTCLIENT::Identity* identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result));

        void Close();

        static U32 STDCALL Process(void* context);
    };
}
