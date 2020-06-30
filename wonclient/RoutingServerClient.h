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

            typedef U32 ClientId;
            typedef StrCrc<32, CH> ClientName;

            // Client data
            struct ClientData {
                ClientId       clientId;
                ClientName     clientName;
                unsigned long  IPAddress;
                unsigned long  mintUserId;
                unsigned long  communityId;
                unsigned short trustLevel;
                bool           isModerator;
                bool           isMuted;

                ClientData() : clientId(0), IPAddress(0), mintUserId(0), communityId(0), trustLevel(0), isModerator(false), isMuted(false) {}
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

                ClientId clientId;
                int type;
                StrCrc<128, CH> text;
                bool isWhisper;
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

        struct Request : CommandRequest {};
        struct Result : CommandResult {};

        ////////////////////////////////////////////////////////////////
        //
        // Connect to a room.
        //

        struct ConnectRoomRequest : Request {};
        struct ConnectRoomResult : Result {};

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        //
        // Register a client.
        //

        struct RegisterClientRequest : Request
        {
            StrCrc<32, CH> username;    // User's login name.
            StrCrc<32, CH> password;    // Room password.
            bool becomeHost;            // Will we join as the host?
            bool becomeSpec;            // Will we join as a spectator?
            bool joinChat;              // Should we join the chat-room when the result comes back.
        };
        struct RegisterClientResult : Result {};

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        struct GetNumUsersResult : Result {};

        ////////////////////////////////////////////////////////////////
        //
        // Get the list of clients connected to this server.
        //

        struct GetClientListRequest : Request {};

        struct GetClientListResult : Result
        {
            Data::ClientDataList clientDataList;
        };

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        //
        //
        //

        struct ASCIIChatMessageRequest : Request
        {
            Data::ASCIIChatMessage chatMessage;
        };

        struct ASCIIChatMessageResult : Result
        {
            Data::ASCIIChatMessage chatMessage;
        };

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        //
        //
        //

        struct CreateGameRequest : Request
        {
            U32 clientId;
            StrCrc<32, CH> name;
        };

        struct CreateGameResult : Result
        {
        };

        struct UpdateGameRequest : Request
        {
        };

        struct UpdateGameResult : Result
        {
        };

        struct DeleteGameRequest : Request
        {
        };

        struct DeleteGameResult : Result
        {
        };

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        // Persistent connection to server.
        MINTCLIENT::Client* client;
        MINTCLIENT::Client::CommandList* command_list;
        
        Win32::EventIndex eventQuit;

        Win32::Thread h_RoutingServerClientThread;

        static const int ID_ASCIIPeerChatCatcher = 0;
        std::map<int, std::list<MINTCLIENT::Client::CommandList*>> catchers;

        RoutingServerClient()
        {
            client = nullptr;
            command_list = new MINTCLIENT::Client::CommandList();
        }

        ~RoutingServerClient()
        {
            command_list->AbortAll();
            this->eventQuit.Signal();
            this->Close();
        }

        WONAPI::Error InstallASCIIPeerChatCatcher(void (*callback)(const RoutingServerClient::ASCIIChatMessageResult& message), void* context);

        WONAPI::Error GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result), void* context);
        WONAPI::Error GetUserList(void (*callback)(const RoutingServerClient::GetClientListResult& result), void* context);

        WONAPI::Error Register(const CH* loginName, const CH* password, bool becomeHost, bool becomeSpec, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result), void* context);
        WONAPI::Error Connect(MINTCLIENT::IPSocket::Address address, MINTCLIENT::Identity* identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result), void* context);

        WONAPI::Error BroadcastChat(std::wstring& text, bool f);

        WONAPI::Error CreateGame(const CH* name, const U32 clientId, void (*callback)(const RoutingServerClient::CreateGameResult& result), void* context);
        WONAPI::Error UpdateGame(const CH* name, const U32 clientId, void (*callback)(const RoutingServerClient::UpdateGameResult& result), void* context);
        WONAPI::Error DeleteGame(const CH* name, const U32 clientId, void (*callback)(const RoutingServerClient::DeleteGameResult& result), void* context);

        U32 GetClientId();

        void Close();

        static U32 STDCALL RoutingServerClientProcess(void* context);
    };
}
