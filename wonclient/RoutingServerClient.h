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

        struct ConnectRoomRequest : Request
        {
            StrCrc<32, CH> username;
            StrCrc<32, CH> password;
        };
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
            StrCrc<32, CH> username;        // User's login name.
            StrCrc<32, CH> room_password;   // Room password.
            bool becomeHost;                // Will we join as the host?
            bool becomeSpec;                // Will we join as a spectator?
            bool joinChat;                  // Should we join the chat-room when the result comes back.
        };
        struct RegisterClientResult : Result {};

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        //
        // Get the list of clients connected to this server.
        //

        struct GetNumUsersResult : Result
        {
            U32 numUsers;
        };

        struct GetUserListRequest : Request {};

        struct GetUserListResult : Result
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

        struct ClientEnterResult : Result
        {
            Data::ClientData client;
        };

        struct ClientUpdateResult : Result
        {
            Data::ClientData client;
        };

        struct ClientLeaveResult : Result
        {
            Data::ClientData client;
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
        // Game Object Creation / Modification / Deletion - Ignoring original WON convention of an abstract Data Object and Opting for concrete StyxNet session data.
        //

        struct CreateGameRequest : Request
        {
            U32 clientId;
            StrCrc<32, CH> name;
            // <StyxNet Session data appended as bytes to payload>
        };

        struct GameResult : Result
        {
            U32 ownerId;
            StrCrc<32, CH> name;
            MINTCLIENT::IPSocket::Address address;
            U8* game_data;
            U32 game_data_size;
        };

        struct CreateGameResult : GameResult {};

        struct UpdateGameRequest : Request
        {
            U32 clientId;
            StrCrc<32, CH> name;
            // <StyxNet Session data appended as bytes to payload>
        };

        struct UpdateGameResult : GameResult {};
        struct ReplaceGameResult : GameResult {};

        // Opposes convention of "GetUserList stored in Data::"
        typedef std::list<GameResult> GameResultList;

        struct GetGameListResult : Result
        {
            GameResultList gameResultList;
        };

        struct DeleteGameRequest : Request
        {
            U32 clientId;
            StrCrc<32, CH> name;
            // <StyxNet Session data appended as bytes to payload>
        };

        struct DeleteGameResult : GameResult {};

        //
        //
        //
        ////////////////////////////////////////////////////////////////

        // Persistent connection to server.
        MINTCLIENT::Client* client;
        MINTCLIENT::Client::CommandList* command_list;
        
        Win32::EventIndex eventQuit;

        Win32::Thread h_RoutingServerClientThread;

        static const int ID_ASCIIPeerChatCatcher    = 0;
        static const int ID_ClientEnterCatcher      = 1;
        static const int ID_ClientLeaveCatcher      = 2;
        static const int ID_GameCreatedCatcher      = 3;
        static const int ID_GameUpdatedCatcher      = 4;
        static const int ID_GameReplacedCatcher     = 5;
        static const int ID_GameDeletedCatcher      = 6;
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

        WONAPI::Error InstallClientEnterCatcher(void (*callback)(const RoutingServerClient::ClientEnterResult& result), void* context);
        WONAPI::Error InstallClientLeaveCatcher(void (*callback)(const RoutingServerClient::ClientLeaveResult& result), void* context);
        WONAPI::Error InstallGameCreatedCatcher(void (*callback)(const RoutingServerClient::CreateGameResult& result), void* context);
        WONAPI::Error InstallGameUpdatedCatcher(void (*callback)(const RoutingServerClient::UpdateGameResult& result), void* context);
        WONAPI::Error InstallGameReplacedCatcher(void (*callback)(const RoutingServerClient::ReplaceGameResult& result), void* context);
        WONAPI::Error InstallGameDeletedCatcher(void (*callback)(const RoutingServerClient::DeleteGameResult& result), void* context);
        WONAPI::Error InstallASCIIPeerChatCatcher(void (*callback)(const RoutingServerClient::ASCIIChatMessageResult& message), void* context);

        WONAPI::Error GetNumUsers(void (*callback)(const RoutingServerClient::GetNumUsersResult& result), void* context);
        WONAPI::Error GetUserList(void (*callback)(const RoutingServerClient::GetUserListResult& result), void* context);

        WONAPI::Error Connect(MINTCLIENT::IPSocket::Address address, MINTCLIENT::Identity& identity, bool isReconnect, long timeout, void (*callback)(const RoutingServerClient::ConnectRoomResult& result), void* context);
        WONAPI::Error Register(const CH* username, const CH* room_password, bool becomeHost, bool becomeSpec, bool joinChat, void (*callback)(const RoutingServerClient::RegisterClientResult& result), void* context);

        WONAPI::Error BroadcastChat(std::wstring& text, bool f);

        WONAPI::Error CreateGame(const CH* name, const U32 clientId, const MINTCLIENT::Client::MINTBuffer& data, void (*callback)(const RoutingServerClient::CreateGameResult& result), void* context);
        WONAPI::Error UpdateGame(const CH* name, const U32 clientId, const MINTCLIENT::Client::MINTBuffer& data, void (*callback)(const RoutingServerClient::UpdateGameResult& result), void* context);
        WONAPI::Error DeleteGame(const CH* name, const U32 clientId, void (*callback)(const RoutingServerClient::DeleteGameResult& result), void* context);

        WONAPI::Error GetGameList(void (*callback)(const RoutingServerClient::GetGameListResult& result), void* context);

        U32 GetClientId();
        Win32::Socket* GetSocket();

        void Close();

        static U32 STDCALL RoutingServerClientProcess(void* context);
    };
}
