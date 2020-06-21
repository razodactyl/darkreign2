#pragma once
#include <map>

#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct RoutingServerClient
    {
        struct Data
        {
            struct ConnectRoomContext
            {
                StrCrc<32> roomName;
                StrCrc<32> password;
                MINTCLIENT::RoutingServerClient* routingServer;
            };

            typedef unsigned short ClientId;
            typedef StrCrc<32, CH> ClientName;

            // Client data
            struct ClientData {
                ClientId       mClientId;
                ClientName     mClientName;
                unsigned long  mIPAddress;
                unsigned long  mWONUserId;
                unsigned long  mCommunityId;
                unsigned short mTrustLevel;
                bool           mIsModerator;
                bool           mIsMuted;

                ClientData() : mClientId(0), mIPAddress(0), mWONUserId(0), mCommunityId(0), mTrustLevel(0), mIsModerator(false), mIsMuted(false) {}
            };

            typedef std::list<ClientData> ClientDataList;

            struct ReadDataObjectResult
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
                static const int CHATTYPE_ASCII_EMOTE = 0x01;
                static const int CHATTYPE_ASCII = 0x02;

                ClientId mSenderId;
                int mChatType;
                std::string mData;

                static bool IsWhisper()
                {
                    return 0;
                }
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

        struct Result : CommandResult {};

        struct ConnectRoomResult : Result {};
        struct RegisterClientResult : Result {};

        struct GetNumUsersResult : Result {};
        struct GetClientListResult : Result
        {
            Data::ClientDataList clientDataList;
        };

        struct ASCIIChatMessageResult : Result
        {
            Data::ASCIIChatMessage chatMessage;
        };

        // Persistent connection to server.
        MINTCLIENT::Client* client;

        static const int ID_ASCIIPeerChatCatcher = 0;
        std::map<int, std::list<MINTCLIENT::Client::ContextList<MINTCLIENT::RoutingServerClient::ASCIIChatMessageResult>*>> catchers;
        WONAPI::Error InstallASCIIPeerChatCatcher(void (*callback)(const RoutingServerClient::ASCIIChatMessageResult& message), void* context);

        WONAPI::Error GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result), void* context);
        WONAPI::Error GetUserList(void (*callback)(const RoutingServerClient::GetClientListResult& result), void* context);

        WONAPI::Error Register(const CH* loginName, const CH* password, bool becomeHost, bool becomeSpectator, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result), void* context);
        WONAPI::Error Connect(MINTCLIENT::IPSocket::Address routingAddress, MINTCLIENT::Identity* identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result), void* context);

        WONAPI::Error BroadcastChat(std::wstring& text, bool f);

        void Close();

        static U32 STDCALL Process(void* context);
    };
}
