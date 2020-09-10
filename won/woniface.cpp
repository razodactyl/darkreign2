///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// Networking WON Stuff
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "defines.h"

#include "win32.h"
#undef _WINSOCKAPI_
#include "woninc.h"

#include "std.h"
#include "woniface.h"
#include "system.h"
#include "queue.h"
#include "utils.h"

#include <vector>
///////////////////////////////////////////////////////////////////////////////
//
// Libraries
//

// #pragma comment(lib, "wsock32.lib")

// #define ALLOW_BETA_KEYS
// #pragma message("ALLOW BETA KEYS")

// #include <enet/enetpp.hxx>
// #include <networking/Networking.hxx>

// #include "../styxnet/win32_dns.h"

///////////////////////////////////////////////////////////////////////////////
//
// Logging
//

#ifdef ALLOW_BETA_KEYS
#define LOG_WON LOG_DIAG
#else
#ifdef DEVELOPMENT
#define LOG_WON LOG_DIAG
#else
#define LOG_WON(x)      \
    if (logging)        \
    {                   \
      LOG_DIAG(x)       \
    }
#endif
#endif


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace WonIface
//
namespace WonIface
{
    LOGDEFLOCAL("WON");

    ///////////////////////////////////////////////////////////////////////////////
    //
    // Constants
    //

    // Default server port number
    static const U16 portNumber = 0x6666;

    // Amount of time to wait for a timeout
    static const S32 requestTimeout = -1;

    // The max users in a lobby to be considered
    static U32 maxLobbyUsers = 127;

    // Logging
    static Bool logging = FALSE;

    // Ignored players
    static BinTree<void> ignoredPlayers;

    // Community
    const char* community = "dr2";

    // Servers
    static const wchar_t* serverAuth = L"AuthServer";
    static const wchar_t* serverRouting = L"RoutingServer";
    static const wchar_t* serverFactory = L"FactoryServer";
    static const wchar_t* serverProfile = L"ProfileServer";
    static const wchar_t* serverFirewall = L"FirewallDetector";

    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct ServerArray
    //
    struct ServerArray
    {
        // Array of servers
        std::vector<MINTCLIENT::IPSocket::Address> servers;

        // Constructor
        ServerArray()
        {
        }

        // Destructor
        ~ServerArray()
        {
        }

        int Length()
        {
            return servers.size();
        }

        // Clear
        void Clear()
        {
            servers.clear();
        }

        // Resize
        void Resize(U16 size)
        {
            Clear();

            if (size)
            {
                servers.resize(size);
            }
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct ChatServer
    //
    struct ChatServer
    {
        // List node
        NBinTree<ChatServer>::Node node;

        // Name of the chat server
        RoomName name;

        // Address of the chat server
        MINTCLIENT::IPSocket::Address address;

        // Num players
        U32 numPlayers;

        // Password protected
        Bool password;

        // Permanent
        Bool permanent;

        // Constructor
        ChatServer(const CH* name, const MINTCLIENT::IPSocket::Address& address, U32 numPlayers, Bool password, Bool permanent)
            : name(name),
            address(address),
            numPlayers(numPlayers),
            password(password),
            permanent(permanent)
        {
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct RoomClient
    //
    struct RoomClient
    {
        // Tree node
        NBinTree<RoomClient>::Node node;

        // Name of the client
        PlayerName name;

        // Id of the client
        U32 id;

        // Is this player a moderator
        Bool moderator;

        // Is this player muted 
        Bool muted;

        // Constructor
        RoomClient(const CH* name, U32 id, Bool moderator, Bool muted)
            : name(name),
            id(id),
            moderator(moderator),
            muted(muted)
        {
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct RoomGame
    //
    struct RoomGame : public Game
    {
        // Tree Node
        NBinTree<RoomGame>::Node treeNode;

        // Constructor
        RoomGame(const GameName& name, const PlayerName& host, U32 size, const U8* data)
            : Game(name, host, size, data)
        {
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct AbortableContet
    //
    struct AbortableContext : public Debug::Memory::UnCached
    {
        // Abort flag
        System::Event abort;

        // List node
        NList<AbortableContext>::Node node;

        // Constructor
        AbortableContext();

        // Destructor
        ~AbortableContext();
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct CreateRoomContext
    //
    struct CreateRoomContext : public AbortableContext
    {
        // Address
        std::wstring address;

        // Port
        U16 port;

        // Room name
        RoomName room;

        // Password
        PasswordStr password;

        // Constructor
        CreateRoomContext(const CH* room, const CH* password)
            : room(room),
            password(password)
        {
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct ConnectRoomContext
    //
    struct ConnectRoomContext : public AbortableContext
    {
        // Room Name
        RoomName roomName;

        // Password
        PasswordStr password;

        // Routing server we're connecting to
        MINTCLIENT::RoutingServerClient* routingServer;

        // Constructor
        ConnectRoomContext
        (
            const CH* roomName,
            const CH* password,
            MINTCLIENT::RoutingServerClient* routingServer
        )
            : roomName(roomName),
            password(password),
            routingServer(routingServer)
        {
        }

        // Destructor
        ~ConnectRoomContext()
        {
            if (routingServer)
            {
                routingServer->Close();
                delete routingServer;
            }
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct RoomClientList
    //
    struct RoomClientList : public NBinTree<RoomClient>
    {
        // Constructor
        RoomClientList()
            : NBinTree<RoomClient>(&RoomClient::node)
        {
        }

        // Add client, return ptr if added, NULL if exists
        RoomClient* AddNoDup(const MINTCLIENT::RoutingServerClient::Data::ClientData& client)
        {
            PlayerName name;
            Utils::Strmcpy(name.str, (const CH*)client.clientName.str, Min<U32>(client.clientName.GetSize() / sizeof(CH) + 1, name.GetSize()));

            RoomClient* rc = Find(client.clientId);

            if (rc)
            {
                // Update the player information
                rc->moderator = client.isModerator;
                rc->muted = client.isMuted;

                LOG_WON(("Updated RoomClient [%s] mod:%d mute:%d", Utils::Unicode2Ansi(name.str), rc->moderator, rc->muted));
            }
            else
            {
                RoomClient* rc = new RoomClient(name.str, client.clientId, client.isModerator, client.isMuted);
                Add(client.clientId, rc);

#ifdef DEVELOPMENT
                in_addr addr;
                addr.s_addr = client.IPAddress;
                LOG_WON(("Added RoomClient [%s] ip:%s mod:%d mute:%d", Utils::Unicode2Ansi(name.str), inet_ntoa(addr), rc->moderator, rc->muted));
#else
                LOG_WON(("Added RoomClient [%s] mod:%d mute:%d", Utils::Unicode2Ansi(name.str), rc->moderator, rc->muted));
#endif

                return (rc);
            }
            return (nullptr);
        }

        // Remove client, return TRUE if existed (and fill the name buffer), FALSE if not
        Bool Remove(U32 clientId, PlayerName& name)
        {
            RoomClient* c = Find(clientId);

            if (c)
            {
                name = c->name;
                Dispose(c);

                LOG_WON(("Removed RoomClient [%s]", Utils::Unicode2Ansi(name.str)));

                return (TRUE);
            }
            return (FALSE);
        }

        // Find a client by name
        RoomClient* FindByName(PlayerName& name)
        {
            for (Iterator i(this); *i; ++i)
            {
                if ((*i)->name == name)
                {
                    return (*i);
                }
            }
            return (nullptr);
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct HTTPdata
    //
    struct HTTPdata
    {
        // Handle
        U32 handle;

        // Has the file been modified
        bool isNew;

        // What is the time on the file
        time_t time;

        // Flag for aborting the transfer
        Bool abort;

        NList<HTTPdata>::Node node;

        HTTPdata(U32 handle)
            : handle(handle),
            isNew(FALSE),
            time(1),
            abort(FALSE)
        {
        }
    };


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Internal Data
    //

    // Initialization flag
    static Bool initialized = FALSE;

    // Connected
    static Bool connected = FALSE;

    // Event queue
    static SafeQueue<Event, 256> eventQueue;

    // Firewall status
    static U32 firewallStatus;

    // Critical section for access data which is shared accross threads
    static System::CritSect critData;

    // Critical section for accessing the current routing server
    static System::CritSect critRouting;

    // Our MINT identity
    static MINTCLIENT::Identity identity;

    // Routing server we are currently dealing with
    static MINTCLIENT::RoutingServerClient* currentRoutingServer = nullptr;

    // Abortable contexts
    static NList<AbortableContext> abortableContexts(&AbortableContext::node);

    // Critsec for modifying the abortable contexts list
    static System::CritSect critAbortable;

    // Directory servers
    static ServerArray directoryServers;

    // Authentication servers
    static ServerArray authServers;

    // Factory servers
    static ServerArray factoryServers;

    // Firewall servers
    static ServerArray firewallServers;

    // Profile servers
    static ServerArray profileServers;

    // Chat servers
    static NBinTree<ChatServer> chatServers(&ChatServer::node);

    // Room clients
    static RoomClientList roomClients;
    static NBinTree<RoomGame> roomGames(&RoomGame::treeNode);

    // Http handle
    static U32 httpHandle;

    // Http data list
    static NList<HTTPdata> httpData(&HTTPdata::node);

    // Http data critical section
    static System::CritSect httpDataCritSect;


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Prototypes
    //
    static void Reset();
    static void ConnectRoom(const MINTCLIENT::IPSocket::Address& address, const CH* room, const CH* password, AbortableContext* context);
    static void PostEvent(U32 message, void* data = nullptr);
    static const char* DecodeMessage(U32 message);

    static MINTCLIENT::RoutingServerClient* CreateRoutingServer();
    static void UpdateRooms(AbortableContext* context);
    static void JoinLobby(AbortableContext* context);

    static void TitanServerListCallback(const MINTCLIENT::Directory::Result& result);

    static void CreateAccountCallback(const MINTCLIENT::Identity::Result& result);
    static void LoginAccountCallback(const MINTCLIENT::Identity::Result& result);
    static void ChangePasswordCallback(const MINTCLIENT::Identity::Result& result);
    static void DetectFirewallCallback(const MINTCLIENT::Firewall::DetectResult& result);

    static void UpdateRoomsCallback(const MINTCLIENT::Directory::Result& result);

    //static void StartTitanServerCallback(const WONAPI::StartTitanServerResult& result, CreateRoomContext*);
    static void ConnectRoomCallback(const MINTCLIENT::RoutingServerClient::ConnectRoomResult& result);
    static void RegisterPlayerCallback(const MINTCLIENT::RoutingServerClient::RegisterClientResult& result);
    static void UserListCallback(const MINTCLIENT::RoutingServerClient::GetUserListResult& result);
    static void HTTPGetCallback(WONAPI::Error result, HTTPdata*);
    static bool HTTPProgressCallback(unsigned long progress, unsigned long size, void* callbackPrivData);

    static void CreateGameCallback(const MINTCLIENT::RoutingServerClient::CreateGameResult& result);
    static void DeleteGameCallback(const MINTCLIENT::RoutingServerClient::DeleteGameResult& result);
    static void UpdateGameCallback(const MINTCLIENT::RoutingServerClient::UpdateGameResult& result);
    static void GameListCallback(const MINTCLIENT::RoutingServerClient::GetGameListResult& result);

    static void GetNumUsersCallback(const MINTCLIENT::RoutingServerClient::GetNumUsersResult& result);

    // MINT Server hooks.

    // static void ClientEnterExCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientDataWithReason& data, void*);
    static void ClientEnterCatcher(const MINTCLIENT::RoutingServerClient::ClientEnterResult& result);
    static void ClientLeaveCatcher(const MINTCLIENT::RoutingServerClient::ClientLeaveResult& result);
    // static void ClientLeaveCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientIdWithReason& data, void*);
    // static void ClientJoinAttemptCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientDataWithReason& reason, void*);
    // static void MuteClientCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientIdWithFlag& reason, void*);
    // static void BecomeModeratorCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientIdWithFlag& reason, void*);
    // static void HostChangeCatcher(MINTCLIENT::RoutingServerClient::Data::ClientId client, void*);
    // static void DataObjectCreationCatcher(const MINTCLIENT::RoutingServerClient::Data::DataObjectWithLifespan& reason, void*);
    static void GameCreatedCatcher(const MINTCLIENT::RoutingServerClient::CreateGameResult& result);
    static void GameUpdatedCatcher(const MINTCLIENT::RoutingServerClient::UpdateGameResult& result);
    static void GameReplacedCatcher(const MINTCLIENT::RoutingServerClient::ReplaceGameResult& result);
    static void GameDeletedCatcher(const MINTCLIENT::RoutingServerClient::DeleteGameResult& result);
    // static void DataObjectDeletionCatcher(const MINTCLIENT::RoutingServerClient::Data::DataObject& reason, void*);
    // static void DataObjectModificationCatcher(const MINTCLIENT::RoutingServerClient::Data::DataObjectModification& reason, void*);
    // static void DataObjectReplacementCatcher(const MINTCLIENT::RoutingServerClient::Data::DataObject& reason, void*);
    // static void KeepAliveCatcher(void*);
    static void ASCIIPeerChatCatcher(const MINTCLIENT::RoutingServerClient::ASCIIChatMessageResult& message);
    // static void UnicodePeerChatCatcher(const MINTCLIENT::RoutingServerClient::Data::UnicodeChatMessage& message, void*);
    // static void ReconnectFailureCatcher(void*);


    //
    // Init
    //
    void Init()
    {
        ASSERT(!initialized);

        LOG_WON(("Initilizing WON"));

        // Initialized WON
        MINTInitialize();

        LOG_WON(("WON Initialized"));

        // We haven't checked our firewall status
        firewallStatus = Firewall::Unchecked;

        // Reset the HTTP handle
        httpHandle = 1;

        // Set initialization flag
        initialized = TRUE;
    }


    //
    // Done
    //
    void Done()
    {
        ASSERT(initialized);

        LOG_WON(("Shutting Down WON"));

        // Reset
        Reset();

        LOG_WON(("Clearing server lists"));

        // Clear auth servers and firewall server
        authServers.Clear();
        firewallServers.Clear();
        profileServers.Clear();

        LOG_WON(("Terminating WON"));

        // Shutdown WON
        MINTTerminate();

        LOG_WON(("Cleaning up abortable contexts"));

        // Delete any remaining abortable contexts (this is slightly evil because it doesn't enter the abortableCrit)
        NList<AbortableContext>::Iterator a(&abortableContexts);
        while (AbortableContext* context = a++)
        {
            delete context;
        }

        LOG_WON(("Clearing Directory Servers"));

        // Clear out directory servers
        directoryServers.Clear();

        LOG_WON(("Aborting any remaining HTTP transfers"));

        // Clear out any remaining http transfers
        LOG_WON(("httpDataCritSect.Wait"));
        httpDataCritSect.Wait();
        httpData.DisposeAll();
        LOG_WON(("httpDataCritSect.Signal"));
        httpDataCritSect.Signal();

        LOG_WON(("WON Shut Down"));

        // Clear ignored players
        ignoredPlayers.UnlinkAll();

        // Clear initialized flag
        initialized = FALSE;
    }


    //
    // Set the directory servers to use
    //
    void SetDirectoryServers(const List<char>& servers)
    {
        LOG_WON(("Setting Directory Servers"));

        ASSERT(servers.GetCount() < U16_MAX);
        directoryServers.Resize(static_cast<U16>(servers.GetCount()));
        U32 index = 0;

        for (List<char>::Iterator s(&servers); *s; ++s)
        {
            LOG_WON(("Added Server %s", *s));
            directoryServers.servers[index++] = MINTCLIENT::IPSocket::Address(*s);
        }
    }


    //
    // Connect
    //
    void Connect()
    {
        // Reset data
        Reset();

        // // Data is the valid versions
        // WONCommon::DataObjectTypeSet data;
        // data.insert(WONCommon::DataObject(dataValidVersions));

        LOG_WON(("Getting Main Server List"));

        LOG_WON(("Calling GetDirectoryEx: %d servers", directoryServers.servers.size()));

        // WONAPI::Error error = WONAPI::GetDirectoryEx
        // (
        //     NULL,                         // Identity* identity
        //     directoryServers.servers,     // const IPSocket::Address* directoryServers
        //     directoryServers.num,         // unsigned int numAddrs
        //     NULL,                         // IPSocket::Address* fromDirServer
        //     dirTitanServers,              // const WONCommon::WONString& path
        //     NULL,                         // WONMsg::DirEntityList* result
        //     WONMsg::GF_DECOMPROOT |       // long flags
        //     WONMsg::GF_DECOMPRECURSIVE |
        //     WONMsg::GF_DECOMPSERVICES |
        //     WONMsg::GF_ADDTYPE |
        //     WONMsg::GF_SERVADDNAME |
        //     WONMsg::GF_SERVADDNETADDR |
        //     WONMsg::GF_ADDDOTYPE |
        //     WONMsg::GF_ADDDODATA,
        //     data,                         // const WONCommon::DataObjectTypeSet& dataSet
        //     0,                            // DirEntityCallback callback
        //     0,                            // void* callbackPrivData
        //     requestTimeout,               // long timeout
        //     TRUE,                         // bool async
        //     TitanServerListCallback,      // void (*f)(const DirEntityListResult&, privsType)
        //     new AbortableContext          // privsType privs
        // );
        //

        // Get the list of default servers
        WONAPI::Error error = MINTCLIENT::Directory::GetDirectory(
            nullptr,
            &directoryServers.servers,
            TitanServerListCallback,
            new AbortableContext
        );

        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            break;

        default:
            ERR_FATAL(("GetDirectoryEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // Disconnect
    //
    void Disconnect()
    {
        LOG_WON(("Disconnecting"));
        Abort();
        Reset();
    }


    //
    // Abort what ever we're doing
    //
    void Abort()
    {
        LOG_WON(("Aborting"));

        critAbortable.Wait();

        // Throw the abort flag in all abortable contexts
        for (NList<AbortableContext>::Iterator a(&abortableContexts); *a; ++a)
        {
            (*a)->abort.Signal();
        }

        critAbortable.Signal();
    }


    //
    // Process
    //
    Bool Process(U32& message, void*& data)
    {
        // Are there any events in the event queue ?
        if (Event* e = eventQueue.RemovePre(0))
        {
            // We got one
            message = e->message;
            data = e->data;
            eventQueue.RemovePost();

            return (TRUE);
        }
        // There were no messages
        return (FALSE);
    }


    //
    // Event queue
    //
    SafeQueue<Event, 256>& GetEventQueue()
    {
        return (eventQueue);
    }


    //
    // Create a Won account
    //
    void CreateAccount(const char* username, const char* password)
    {
        LOG_WON(("Create Identity: Username %s Community %s AuthServers %d", username, community, authServers.Length()));

        // Build an Identity
        // identity = WONAPI::Identity(username, community, password, "", authServers.servers, authServers.num);
        identity.Set(
            Utils::Strdup(Utils::Ansi2Unicode(username)),
            Utils::Strdup(Utils::Ansi2Unicode(password))
        );

        // Ask WON to create the account
        // WONAPI::Error error = identity.AuthenticateNewAccountEx
        // (
        //     requestTimeout,
        //     TRUE,
        //     CreateAccountCallback,
        //     new AbortableContext
        // );

        auto c = new MINTCLIENT::Client::Config(authServers.servers.front());
        auto client = new MINTCLIENT::Client(*c);

        WONAPI::Error error = identity.Create(
            client,
            CreateAccountCallback,
            new AbortableContext
        );

        // Ensure the operation started
        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            break;

        default:
            ERR_FATAL(("Identity::AuthenticateNewAccountEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // CreateAccountCallback
    //
    void CreateAccountCallback(const MINTCLIENT::Identity::Result& result)
    {
        auto context = static_cast<AbortableContext*>(result.context);
        ASSERT(context);

        LOG_WON(("CreateAccountCallback in"));
        LOG_WON(("AuthenticateNewAccount CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        if (context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
            case WONAPI::Error_Success:
                PostEvent(Message::CreatedAccount);
                break;

            case MINTCLIENT::Error::IdentityAlreadyExists:
                PostEvent(Error::CreateAccountBadUser);
                break;

                // case WONMsg::StatusAuth_ExpiredKey:
                //     PostEvent(Error::KeyExpired);
                //     break;
                //
                // case WONMsg::StatusAuth_VerifyFailed:
                //     PostEvent(Error::VerifyFailed);
                //     break;
                //
                // case WONMsg::StatusAuth_LockedOut:
                //     PostEvent(Error::LockedOut);
                //     break;
                //
                // case WONMsg::StatusAuth_KeyInUse:
                //     PostEvent(Error::KeyInUse);
                //     break;
                //
                // case WONMsg::StatusAuth_InvalidCDKey:
                //     PostEvent(Error::KeyInvalid);
                //     break;

            default:
                LOG_ERR(("AuthenticateNewAccount CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                PostEvent(Error::CreateAccountFailure);
                break;
            }
        }

        delete context;

        LOG_WON(("CreateAccountCallback out"));
    }

    //
    // Login to a Won account
    //
    void LoginAccount(const CH* username, const CH* password)
    {
        LOG_WON(("Login Identity: Username %s Community %s AuthServers %d", Utils::Unicode2Ansi(username), community, authServers.Length()));

        // Build an Identity
        identity.Set(username, password);

        // identity = WONAPI::Identity
        // (
        //     username,
        //     community,
        //     password,
        //     "",
        //     authServers.servers,
        //     authServers.Length()
        // );

        auto c = new MINTCLIENT::Client::Config(authServers.servers.front());
        auto client = new MINTCLIENT::Client(*c);

        // Ask WON to authenticate the account
        WONAPI::Error error = identity.Authenticate(
            client,
            LoginAccountCallback,
            new AbortableContext
        );

        // WONAPI::Error error = identity.AuthenticateEx
        // (
        //     FALSE,
        //     FALSE,
        //     requestTimeout,
        //     TRUE,
        //     LoginAccountCallback,
        //     new AbortableContext
        // );

        // Ensure the operation started
        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            // case WONMsg::StatusAuth_InvalidCDKey:
            break;

        default:
            ERR_FATAL(("Identity::AuthenticateEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // LoginAccountCallback
    //
    void LoginAccountCallback(const MINTCLIENT::Identity::Result& result)
    {
        auto context = static_cast<AbortableContext*>(result.context);
        ASSERT(context);

        LOG_WON(("LoginAccountCallback in"));
        LOG_WON(("Authenticate CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        if (context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
            case WONAPI::Error_Success:
            {
                PostEvent(Message::LoggedIn);

                identity.SetLoggedIn(true);

                // Begin room update workflow.
                UpdateRooms(context);

                // Context has been passed to update rooms
                context = NULL;
            }
            break;

            case MINTCLIENT::Error::IdentityAuthenticationFailed:
            {
                PostEvent(Error::LoginInvalidPassword);
            }
            break;

            // case WONMsg::StatusAuth_UserNotFound:
            //     PostEvent(Error::LoginInvalidUsername);
            //     break;
            //
            // case WONMsg::StatusAuth_BadPassword:
            //     PostEvent(Error::LoginInvalidPassword);
            //     break;
            //
            // case WONMsg::StatusAuth_ExpiredKey:
            //     PostEvent(Error::KeyExpired);
            //     break;
            //
            // case WONMsg::StatusAuth_VerifyFailed:
            //     PostEvent(Error::VerifyFailed);
            //     break;
            //
            // case WONMsg::StatusAuth_LockedOut:
            //     PostEvent(Error::LockedOut);
            //     break;
            //
            // case WONMsg::StatusAuth_KeyInUse:
            //     PostEvent(Error::KeyInUse);
            //     break;
            //
            // case WONMsg::StatusAuth_InvalidCDKey:
            //     PostEvent(Error::KeyInvalid);
            //     break;

            default:
                LOG_ERR(("Authenticate CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                PostEvent(Error::LoginFailure);
                break;
            }
        }

        //delete context;

        LOG_WON(("LoginAccountCallback out"));
    }


    //
    // Change the password of an existing Won account
    //
    void ChangePassword(const char* username, const char* oldPassword, const char* newPassword)
    {
        LOG_WON(("Change Identity: Username %s Community %s AuthServers %d", username, community, authServers.Length()));

        // Build an Identity
        identity.Set(Utils::Ansi2Unicode(Utils::Strdup(username)), Utils::Ansi2Unicode(Utils::Strdup(oldPassword)));
        identity.SetNewPassword(Utils::Ansi2Unicode(Utils::Strdup(newPassword)));

        auto c = new MINTCLIENT::Client::Config(authServers.servers.front());
        auto client = new MINTCLIENT::Client(*c);

        // Ask MINT to update this account.
        WONAPI::Error error = identity.Update
        (
            client,
            ChangePasswordCallback,
            new AbortableContext
        );

        // Ensure that the operation started
        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            // case WONMsg::StatusAuth_InvalidCDKey:
            break;

        case WONAPI::Error_BadNewPassword:
            PostEvent(Error::ChangePasswordBadNewPassword);
            break;

        default:
            ERR_FATAL(("Identity::AuthenticateNewPasswordEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // ChangePasswordCallback
    //
    void ChangePasswordCallback(const MINTCLIENT::Identity::Result& result)
    {
        LOG_WON(("ChangePasswordCallback in"));
        LOG_WON(("ChangePasswordCallback CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        auto context = static_cast<AbortableContext*>(result.context);
        ASSERT(context);

        if (context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
            case WONAPI::Error_Success:
                PostEvent(Message::ChangedPassword);
                break;

            default:
                LOG_ERR(("ChangePasswordCallback CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                PostEvent(Error::ChangePasswordFailure);
                break;
            }
        }

        delete context;

        LOG_WON(("ChangePasswordCallback out"));
    }


    //
    // DetectFirewall
    //
    void DetectFirewall()
    {
        auto result = MINTCLIENT::Firewall::DetectResult();
        result.error = WONAPI::Error_Success;
        result.behindFirewall = false;

        DetectFirewallCallback(result);

        // LOG_WON(("Detecting Firewall"));
        //
        // // We are now checking for a firewall
        // firewallStatus = Firewall::Checking;
        //
        // LOG_WON(("DetectFirewallEx: Num Servers %d", firewallServers.num));
        //
        // // Detect the presence of the a firewall
        // WONAPI::Error error = WONAPI::DetectFirewallEx
        // (
        //     firewallServers.servers,
        //     firewallServers.num,
        //     NULL,
        //     portNumber,
        //     requestTimeout,
        //     TRUE,
        //     DetectFirewallCallback,
        //     (void*)NULL
        // );
        //
        // switch (error)
        // {
        // case WONAPI::Error_Success:
        // case WONAPI::Error_Pending:
        //     break;
        //
        // default:
        //     ERR_FATAL(("DetectFirewallEx: %d %s", error, WONErrorToString(error)));
        // }
    }


    //
    // DetectFirewallCallback
    //
    void DetectFirewallCallback(const MINTCLIENT::Firewall::DetectResult& result)
    {
        LOG_WON(("DetectFirewallCallback in"));
        LOG_WON(("DetectFirewallCallback CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
            firewallStatus = result.behindFirewall ? Firewall::Behind : Firewall::None;
            PostEvent(Message::FirewallStatus);
            break;

        default:
            LOG_ERR(("DetectFirewallEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            firewallStatus = Firewall::Unchecked;
            break;
        }

        LOG_WON(("DetectFirewallCallback out"));
    }


    //
    // Are we detected as being behind a firewall
    //
    U32 GetFirewallStatus()
    {
        return (firewallStatus);
    }


    //
    // Keep our Won connection alive
    //
    void KeepAlive()
    {
        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        LOG_WON(("Keeping connection alive using GetNumUsersEx"));

        if (currentRoutingServer)
        {
            // currentRoutingServer->GetNumUsersEx
            // (
            //     0,                           // unsigned short theTag
            //     GetNumUsersCallback,         // void (*f)(const GetNumUsersResult&, privsType t)
            //     static_cast<void*>(nullptr)  // privsType t
            // );
            currentRoutingServer->GetNumUsers(
                GetNumUsersCallback,
                nullptr
            );
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();
    }


    //
    // Get the current list of rooms
    //
    void GetRoomList(NList<Room>& rooms)
    {
        // Wait for data mutex
        LOG_WON(("GetRoomList critData.Wait"));
        critData.Wait();

        // Make sure the room list is clear
        rooms.DisposeAll();

        // Transfer the list of rooms over
        for (NBinTree<ChatServer>::Iterator c(&chatServers); *c; ++c)
        {
            rooms.Append(new Room((*c)->name.str, (*c)->numPlayers, (*c)->password, (*c)->permanent));
        }

        // Signal data mutex
        LOG_WON(("GetRoomList critData.Signal"));
        critData.Signal();
    }


    //
    // Create a room
    //
    void CreateRoom(const CH* roomname, const CH* password)
    {
        // LOG_WON(("Creating Room: %s", Utils::Unicode2Ansi(roomname)));
        //
        // if (!identity.GetLoginName().size())
        // {
        //     ERR_FATAL(("No Identity!"));
        // }
        //
        // if (factoryServers.num == 0)
        // {
        //     // There are no factory servers
        //
        // }
        // else
        // {
        //     wstring commandLine;
        //     if (password && password[0] != '\0')
        //     {
        //         commandLine += L"-Password \"";
        //         commandLine += password;
        //         commandLine += L"\" ";
        //     }
        //     commandLine += L"-DirRegDisplayName \"";
        //     commandLine += roomname;
        //     commandLine += L"\" ";
        //
        //     CreateRoomContext* context = new CreateRoomContext(roomname, password);
        //
        //     WONAPI::Error error = WONAPI::StartTitanServerEx
        //     (
        //         &identity,                // Identity* identity
        //         factoryServers.servers,   // const IPSocket::Address* factories
        //         factoryServers.num,       // unsigned int numFactories
        //         &context->address,        // IPSocket::Address* startedOnFactory
        //         chatConfiguration,        // const string& configurationName
        //         &context->port,           // unsigned short* startedOnPorts
        //         NULL,                     // unsigned short* numStartedPorts
        //         commandLine,              // const WONCommon::WONString& commandLineFragment
        //         FALSE,                    // bool replaceCommandLine
        //         1,                        // unsigned char numPortsRequested
        //         0,                        // unsigned char numSpecificPorts
        //         NULL,                     // unsigned short* specificPortsArray
        //         NULL,                     // const IPSocket::Address* authorizedIPs
        //         0,                        // unsigned short numAuthorizedIPs
        //         requestTimeout,           // long timeout
        //         TRUE,                     // bool async
        //         StartTitanServerCallback, // void (*f)(const StartTitanServerResult&, privsType)
        //         context                   // privsType t
        //     );
        //
        //     switch (error)
        //     {
        //     case WONAPI::Error_Success:
        //     case WONAPI::Error_Pending:
        //         break;
        //
        //     default:
        //         ERR_FATAL(("StartTitanServerEx: %d %s", error, WONErrorToString(error)));
        //     }
        // }
    }


    //
    // StartTitanServerCallback
    //
    void StartTitanServerCallback(const MINTCLIENT::Directory::Result& result, CreateRoomContext* context)
    {
        // ASSERT(context);
        //
        // LOG_WON(("StartTitanServerCallback in"));
        // LOG_WON(("StartTitanServerEx CB: %d %s", result.error, WONErrorToString(result.error)));
        //
        // if (context->abort.Test())
        // {
        //     LOG_WON(("Was Aborted"));
        // }
        // else
        // {
        //     switch (result.error)
        //     {
        //     case WONAPI::Error_Success:
        //     {
        //         PostEvent(Message::CreatedRoom);
        //
        //         context->address.SetPort(context->port);
        //         LOG_WON(("Started Server: %s", context->address.GetAddressString(TRUE).c_str()));
        //
        //         // Connect to newly created room
        //         ConnectRoom(context->address, context->room.str, context->password.str, context);
        //         context = NULL;
        //         break;
        //     }
        //
        //     default:
        //         LOG_ERR(("StartTitanServerEx CB: %d %s", result.error, WONErrorToString(result.error)))
        //             PostEvent(Error::CreateRoomFailure);
        //         break;
        //     }
        // }
        //
        // if (context)
        // {
        //     delete context;
        // }
        //
        // LOG_WON(("StartTitanServerCallback out"));
    }


    //
    // Join a room
    //
    void JoinRoom(const CH* roomname, const CH* password)
    {
        LOG_WON(("Joining Room '%s' '%s'", Utils::Unicode2Ansi(roomname), password ? "password" : ""));

        if (!identity.LoggedIn())
        {
            ERR_FATAL(("No Identity!"));
        }

        // Wait for data mutex
        LOG_WON(("critData.Wait"));
        critData.Wait();

        // Attempt to find the room in the chat server tree
        ChatServer* server = chatServers.Find(Crc::CalcStr(roomname));

        // Signal data mutex
        LOG_WON(("critData.Signal"));
        critData.Signal();

        // Verify that we're not currently in that room
        LOG_WON(("currentRoutingServer=0x%.8X", currentRoutingServer));

        if (server)
        {
            ConnectRoom(server->address, roomname, password, NULL);
        }
        else
        {
            LOG_WON(("Room '%s' not found", roomname));
            PostEvent(Error::NoRoom);
        }
    }


    //
    // Get the list of players in the current room
    //
    void GetPlayerList(NList<Player>& players)
    {
        // Wait for data mutex
        LOG_WON(("critData.Wait"));
        critData.Wait();

        // Make sure the room list is clear
        players.DisposeAll();

        // Transfer the list of rooms over
        for (RoomClientList::Iterator c(&roomClients); *c; ++c)
        {
            players.Append(new Player((*c)->name.str, (*c)->id, (*c)->moderator, (*c)->muted, ignoredPlayers.Exists((*c)->name.crc)));
        }

        // Signal data mutex
        LOG_WON(("critData.Signal"));
        critData.Signal();
    }


    //
    // Add a game to the list of games
    //
    void AddGame(const GameName& name, U32 size, const U8* session_data)
    {
        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        if (currentRoutingServer)
        {
            // currentRoutingServer->CreateDataObjectEx
            // (
            //     WONAPI::RoutingServerClient::GROUPID_ALLUSERS,   // ClientOrGroupId theLinkId
            //     dataGamePrefix + (U8*)name.str,                  // const WONCommon::RawBuffer& theDataTypeR
            //     currentRoutingServer->GetClientId(),             // ClientOrGroupId theOwnerId
            //     0,                                               // unsigned short theLifespan
            //     data,                                            // const WONCommon::RawBuffer& theDataR
            //     CreateDataObjectCallback,                        // void (*f)(short, privsType)
            //     (void*)NULL                                      // privsType t)
            // );

            MINTCLIENT::Client::MINTBuffer data;
            data.assign(session_data, size);

            currentRoutingServer->CreateGame(
                Utils::Ansi2Unicode(name.str),
                currentRoutingServer->GetClientId(),
                data,
                CreateGameCallback,
                nullptr
            );
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();
    }


    //
    // Update a game
    //
    void UpdateGame(const GameName& name, U32 size, const U8* session_data)
    {
        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        if (currentRoutingServer)
        {
            // currentRoutingServer->ReplaceDataObjectEx
            // (
            //     WONAPI::RoutingServerClient::GROUPID_ALLUSERS,  // ClientOrGroupId theLinkId
            //     dataGamePrefix + (U8*)name.str,               // const WONCommon::RawBuffer& theDataTypeR
            //     data,                                           // const WONCommon::RawBuffer& theDataR
            //     ReplaceDataObjectCallback,                      // void (*f)(short, privsType)
            //     (void*)NULL                                   // privsType t
            // );

            MINTCLIENT::Client::MINTBuffer data;
            data.assign(session_data, size);

            currentRoutingServer->UpdateGame(
                Utils::Ansi2Unicode(name.str),
                currentRoutingServer->GetClientId(),
                data,
                UpdateGameCallback,
                nullptr
            );
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();
    }


    //
    // Remove a game from the list of games
    //
    void RemoveGame(const GameName& name)
    {
        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        if (currentRoutingServer)
        {
            // currentRoutingServer->DeleteDataObjectEx
            // (
            //     WONAPI::RoutingServerClient::GROUPID_ALLUSERS,  // ClientOrGroupId theLinkId, 
            //     dataGamePrefix + (U8*)name.str,               // const WONCommon::RawBuffer& theDataTypeR,
            //     DeleteDataObjectCallback,                       // void (*f)(short, privsType), 
            //     (void*)NULL                                   // privsType t)
            // );

            currentRoutingServer->DeleteGame(
                Utils::Ansi2Unicode(name.str),
                currentRoutingServer->GetClientId(),
                DeleteGameCallback,
                nullptr
            );
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();
    }


    //
    // Get the list of games in the current room
    // - won_cmd.cpp -> won.rebuildgames -> Extract list of games into `&games`
    //
    void GetGameList(NList<Game>& games)
    {
        // Wait for data mutex
        LOG_WON(("GetGameList critData.Wait"));
        critData.Wait();

        // Make sure the game list is clear
        games.DisposeAll();

        // Transfer the list of games over
        for (NBinTree<RoomGame>::Iterator g(&roomGames); *g; ++g)
        {
            games.Append(new Game((*g)->name, (*g)->hostUsername, (*g)->size, (*g)->data));
        }

        // Signal data mutex
        LOG_WON(("GetGameList critData.Signal"));
        critData.Signal();
    }


    //
    // Get the address which is connecting us to the internet
    //
    Bool GetLocalAddress(U32& ip, U16& port)
    {
        if (currentRoutingServer)
        {
            Win32::Socket* socket = currentRoutingServer->GetSocket();

            if (socket)
            {
                if (socket->IsValid())
                {
                    Win32::Socket::Address addr;
                    if (socket->GetLocalAddress(addr))
                    {
                        ip = ntohl(addr.sin_addr.s_addr);
                        port = ntohs(addr.sin_port);
                        return (TRUE);
                    }
                }
            }
        }

        return (FALSE);
    }


    //
    // Send broadcast chat message
    //
    void BroadcastMessage(const CH* text)
    {
        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        if (currentRoutingServer)
        {
            currentRoutingServer->BroadcastChat(std::wstring((wchar_t*)text), FALSE);
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();
    }


    //
    // Send emote chat message
    //
    //void DLL_DECL EmoteMessage(const CH* text)
    void EmoteMessage(const CH* text)
    {
        LOG_DIAG(("MINT EmoteMessage not implemented."));
        // LOG_WON(("critRouting.Wait"));
        // critRouting.Wait();
        //
        // if (currentRoutingServer)
        // {
        //     wstring message(text);
        //     currentRoutingServer->BroadcastChat(
        //         WONAPI::RoutingServerClient::Message
        //         (
        //             reinterpret_cast<const unsigned char*>(message.data()),
        //             U16(message.size() * sizeof(CH))
        //         ),
        //         WONMsg::CHATTYPE_UNICODE_EMOTE,
        //         FALSE);
        // }
        //
        // LOG_WON(("critRouting.Signal"));
        // critRouting.Signal();
    }


    //
    // Send private chat message
    //
    void PrivateMessage(const char* player, const CH* text)
    {
        LOG_DIAG(("MINT PrivateMessage not implemented."));
        // LOG_WON(("critRouting.Wait"));
        // critRouting.Wait();
        //
        // if (currentRoutingServer)
        // {
        //     PlayerName name;
        //     Utils::Ansi2Unicode(name.str, name.GetSize(), player);
        //     name.Update();
        //
        //     static CH buff[512];
        //     Utils::Strcpy(buff, text);
        //
        //     LOG_WON(("critData.Wait"));
        //     critData.Wait();
        //
        //     // Get the Id of the player
        //     RoomClient* client = roomClients.FindByName(name);
        //
        //     if (client)
        //     {
        //         WONAPI::RoutingServerClient::ClientOrGroupId ids[1];
        //         ids[0] = U16(client->id);
        //         wstring message(buff);
        //
        //         currentRoutingServer->WhisperChat
        //         (
        //             ids,                                  // const ClientOrGroupId theRecipients[]
        //             1,                                    // unsigned short theNumRecipients
        //             TRUE,                                 // bool flagIncludeExclude
        //             WONAPI::RoutingServerClient::Message  // const Message& theMessageR
        //             (
        //                 reinterpret_cast<const unsigned char*>(message.data()),
        //                 U16(message.size() * sizeof(CH))
        //             ),
        //             WONMsg::ChatType(CHATTYPE_UNICODE),   // WONMsg::ChatType theChatType
        //             FALSE                                 // bool shouldSendReply
        //         );
        //     }
        //
        //     LOG_WON(("critData.Signal"));
        //     critData.Signal();
        // }
        //
        // LOG_WON(("critRouting.Signal"));
        // critRouting.Signal();
    }


    //
    // Ignore a player
    //
    void IgnorePlayer(const char* player)
    {
        PlayerName name;
        Utils::Ansi2Unicode(name.str, name.GetSize(), player);
        name.Update();
        IgnorePlayer(name);
    }


    void IgnorePlayer(const PlayerName& name)
    {
        // Signal data mutex
        LOG_WON(("IgnorePlayer critData.Wait"));
        critData.Wait();

        // Clear ignored players
        if (!ignoredPlayers.Exists(name.crc))
        {
            ignoredPlayers.Add(name.crc, NULL);
        }

        // Signal data mutex
        LOG_WON(("IgnorePlayer critData.Signal"));
        critData.Signal();

        PostEvent(Message::PlayersChanged);
    }


    //
    // Unignore a player
    //
    void UnignorePlayer(const char* player)
    {
        PlayerName name;
        Utils::Ansi2Unicode(name.str, name.GetSize(), player);
        name.Update();
        UnignorePlayer(name);
    }


    void UnignorePlayer(const PlayerName& name)
    {
        // Signal data mutex
        LOG_WON(("UnignorePlayer critData.Wait"));
        critData.Wait();

        // Remove the player from the ignored players
        ignoredPlayers.Unlink(name.crc);

        // Signal data mutex
        LOG_WON(("UnignorePlayer critData.Signal"));
        critData.Signal();

        PostEvent(Message::PlayersChanged);
    }


    //
    // Check the key in the registry
    //
    Bool CheckStoredKey()
    {
        return (TRUE);
        //         WONCDKey::ClientCDKey key(productName);
        //
        //         key.Load();
        //
        // #ifdef ALLOW_BETA_KEYS
        //         if (key.IsValid())
        // #else
        //         if (key.IsValid() && !key.IsBeta())
        // #endif
        //         {
        //             // Set up the Authentication API for CD key usage
        //             WONAPI::Identity::SetCDKey(key);
        //             WONAPI::Identity::SetLoginKeyFile(loginKey);
        //             if (!WONAPI::Identity::LoadVerifierKeyFromFile(publicKey))
        //             {
        //                 ERR_FATAL(("LoadVerifierKeyFromFile failed"));
        //             }
        //
        //             return (TRUE);
        //         }
        //         else
        //         {
        //             return (FALSE);
        //         }
    }


    //
    // Check the given key
    //
    Bool CheckKey(const char* text)
    {
        return (TRUE);

        //         WONCDKey::ClientCDKey key(productName);
        //         key.Init(text);
        //
        // #ifdef ALLOW_BETA_KEYS
        //         if (key.IsValid())
        // #else
        //         if (key.IsValid() && !key.IsBeta())
        // #endif
        //         {
        //             key.Save();
        //
        //             // Set up the Authentication API for CD key usage
        //             WONAPI::Identity::SetCDKey(key);
        //             WONAPI::Identity::SetLoginKeyFile(loginKey);
        //             if (!WONAPI::Identity::LoadVerifierKeyFromFile(publicKey))
        //             {
        //                 ERR_FATAL(("LoadVerifierKeyFromFile failed"));
        //             }
        //
        //             return (TRUE);
        //         }
        //         else
        //         {
        //             return (FALSE);
        //         }
    }


    //
    // Retrieve a HTTP file and store it locally in the download directory
    //
    //U32 HTTPGet(U32 proxyIP, U16 proxyPort, const char *hostName, U16 hostPort, const char *path, const char *local, Bool allowResume)
    U32 HTTPGet(const char* proxy, const char* hostName, U16 hostPort, const char* path, const char* local, Bool allowResume)
    {
        LOG_WON(("HTTPGet: %s %s %d %s %s %d", proxy, hostName, hostPort, path, local, allowResume));

        HTTPdata* data = new HTTPdata(httpHandle++);

        // WONAPI::HTTPGet
        // (
        //     WONAPI::IPSocket::Address(proxy), // const IPSocket::Address& proxyAddr,
        //     hostName,                         // const std::string& hostName
        //     hostPort,                         // unsigned short httpPort
        //     path,                             // const std::string& getPath 
        //     local,                            // const std::string& saveAsFile
        //     &data->isNew,                     // bool* isNew
        //     &data->time,                      // time_t* modTime
        //     allowResume ? true : false,       // bool allowResume
        //     HTTPProgressCallback,             // ProgressCallback callback
        //     data,                             // void* callbackPrivData
        //     requestTimeout,                   // long timeout
        //     TRUE,                             // bool async
        //     HTTPGetCallback,                  // void (*f)(Error, privsType), 
        //     data                              // privsType privs
        // );

        LOG_WON(("httpDataCritSect.Wait"));
        httpDataCritSect.Wait();
        httpData.Append(data);
        LOG_WON(("httpDataCritSect.Signal"));
        httpDataCritSect.Signal();

        LOG_WON(("HTTPGet: handle %d", data->handle));

        return (data->handle);
    }


    //
    // Retrieve a HTTP file and store it locally in the download directory
    //
    //U32 DLL_DECL HTTPGet(const char* hostName, U16 hostPort, const char* path, const char* local, Bool allowResume)
    U32 HTTPGet(const char* hostName, U16 hostPort, const char* path, const char* local, Bool allowResume)
    {
        LOG_WON(("HTTPGet: %s %d %s %s %d", hostName, hostPort, path, local, allowResume));

        HTTPdata* data = new HTTPdata(httpHandle++);

        // WONAPI::HTTPGet
        // (
        //     hostName,                         // const std::string& hostName
        //     hostPort,                         // unsigned short httpPort
        //     path,                             // const std::string& getPath 
        //     local,                            // const std::string& saveAsFile
        //     &data->isNew,                     // bool* isNew
        //     &data->time,                      // time_t* modTime
        //     allowResume ? true : false,       // bool allowResume
        //     HTTPProgressCallback,             // ProgressCallback callback
        //     data,                             // void* callbackPrivData
        //     requestTimeout,                   // long timeout
        //     TRUE,                             // bool async
        //     HTTPGetCallback,                  // void (*f)(Error, privsType), 
        //     data                              // privsType privs
        // );

        LOG_WON(("httpDataCritSect.Wait"));
        httpDataCritSect.Wait();
        httpData.Append(data);
        LOG_WON(("httpDataCritSect.Signal"));
        httpDataCritSect.Signal();

        LOG_WON(("HTTPGet: handle %d", data->handle));

        return (data->handle);
    }


    //
    // Abort a HTTP get
    //
    void HTTPAbortGet(U32 handle)
    {
        LOG_WON(("HTTPGet: aborting %d", handle));

        LOG_WON(("httpDataCritSect.Wait"));
        httpDataCritSect.Wait();

        for (NList<HTTPdata>::Iterator d(&httpData); *d; ++d)
        {
            if ((*d)->handle == handle)
            {
                (*d)->abort = TRUE;
                break;
            }
        }

        LOG_WON(("httpDataCritSect.Signal"));
        httpDataCritSect.Signal();
    }


    //
    // Enable / Disable logging
    //
    void Logging(Bool on)
    {
        logging = on;
    }


    //
    // Reset
    //
    void Reset()
    {
        LOG_WON(("Reset in"));

        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();

        if (currentRoutingServer)
        {
            LOG_WON(("Deleting current routing server"));

            currentRoutingServer->Close();
            delete currentRoutingServer;
            currentRoutingServer = nullptr;
        }

        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();

        // Wait for data mutex
        LOG_WON(("Reset critData.Wait"));
        critData.Wait();

        // Cleanup
        factoryServers.Clear();
        chatServers.DisposeAll();
        roomClients.DisposeAll();
        roomGames.DisposeAll();

        // Clear ignored players
        ignoredPlayers.UnlinkAll();

        // Signal data mutex
        LOG_WON(("Reset critData.Signal"));
        critData.Signal();

        LOG_WON(("Reset out"));
    }


    //
    // CreateRoutingServer
    //
    MINTCLIENT::RoutingServerClient* CreateRoutingServer()
    {
        auto* routingServer = new MINTCLIENT::RoutingServerClient();

        // Setup catchers for routing server
        routingServer->InstallClientEnterCatcher(ClientEnterCatcher, nullptr);
        routingServer->InstallClientLeaveCatcher(ClientLeaveCatcher, nullptr);

        // routingServer->InstallClientJoinAttemptCatcherEx(ClientJoinAttemptCatcher, (void *) NULL);
        // routingServer->InstallMuteClientCatcherEx(MuteClientCatcher, (void*)NULL);
        // routingServer->InstallBecomeModeratorCatcherEx(BecomeModeratorCatcher, (void*)NULL);
        // routingServer->InstallHostChangeCatcherEx(HostChangeCatcher, (void *) NULL);

        routingServer->InstallGameCreatedCatcher(GameCreatedCatcher, nullptr);
        routingServer->InstallGameUpdatedCatcher(GameUpdatedCatcher, nullptr);
        routingServer->InstallGameReplacedCatcher(GameReplacedCatcher, nullptr);
        routingServer->InstallGameDeletedCatcher(GameDeletedCatcher, nullptr);

        // routingServer->InstallKeepAliveCatcherEx(KeepAliveCatcher, (void*)NULL);

        routingServer->InstallASCIIPeerChatCatcher(ASCIIPeerChatCatcher, (void*)NULL);
        // routingServer->InstallUnicodePeerChatCatcherEx(UnicodePeerChatCatcher, (void*)NULL);
        // routingServer->InstallReconnectFailureCatcherEx(ReconnectFailureCatcher, (void*)NULL);

        // Return the new routing server
        return (routingServer);
    }


    //
    // Connect to a room
    //
    void ConnectRoom(const MINTCLIENT::IPSocket::Address& address, const CH* roomName, const CH* password, AbortableContext* context)
    {
        MINTCLIENT::RoutingServerClient* routingServer = CreateRoutingServer();
        ConnectRoomContext* connectRoomContext = new ConnectRoomContext(roomName, password, routingServer);

        // If there's a context transfer its abort status
        if (context)
        {
            critAbortable.Wait();
            if (context->abort.Test())
            {
                connectRoomContext->abort.Signal();
            }
            critAbortable.Signal();

            delete context;
            context = nullptr;
        }

        // Connect to the new server
        LOG_DEV(("$$$ ConnectRoom -> Routing Server -> Connect."));
        WONAPI::Error error = routingServer->Connect
        (
            // address,                // const TCPSocket::Address& theRoutingAddress
            // &identity,              // Identity* theIdentityP
            // FALSE,                  // bool isReconnectAttempt
            // requestTimeout,         // long theTimeout
            // TRUE,                   // bool async
            // ConnectRoomCallback,    // void (*f)(Error, privsType)
            // connectRoomContext      // privsType t
            address,
            identity,
            FALSE,
            requestTimeout,
            ConnectRoomCallback,
            connectRoomContext
        );

        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            break;

        default:
            ERR_FATAL(("ConnectEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // ConnectRoomCallback
    //
    void ConnectRoomCallback(const MINTCLIENT::RoutingServerClient::ConnectRoomResult& result)
    {
        auto context = static_cast<ConnectRoomContext*>(result.context);
        ASSERT(context);

        LOG_WON(("ConnectRoomCallback in ->: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        if (context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
            case WONAPI::Error_Success:
            {
                // From our Identity, compose our username
                // WONCommon::RawBuffer loginName;
                // loginName.assign
                // (
                //     (const U8*)identity.GetLoginName().GetUnicodeString().data(),
                //     (identity.GetLoginName().GetUnicodeString().size() + 1) * 2
                // );

                // context->routingServer->RegisterEx
                // (
                //     loginName,                              // const ClientName& theClientNameR
                //     std::wstring(context->password.str),    // const std::string& thePasswordR
                //     FALSE,                                  // bool becomeHost
                //     FALSE,                                  // bool becomeSpectator
                //     TRUE,                                   // bool joinChat
                //     RegisterPlayerCallback,                 // void (*f)(const RegisterClientResult&, privsType)
                //     context                                 // privsType t
                // );

                LOG_DEV(("ConnectRoomCallback -> Routing Server -> Register..."));
                context->routingServer->Register(
                    identity.GetLoginName(),
                    context->password.str,
                    FALSE,
                    FALSE,
                    TRUE,
                    RegisterPlayerCallback,
                    context
                );

                // Context is no longer our problem, problem of the `RegisterPlayerCallback`.
                context = nullptr;

                // Notify of successful connection to room
                PostEvent(Message::ConnectedRoom);
                break;
            }

            case WONAPI::Error_HostUnreachable:
                PostEvent(Error::JoinRoomFailure);
                break;

            default:
                LOG_ERR(("ConnectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                PostEvent(Error::JoinRoomFailure);
                break;
            }
        }

        delete context;

        LOG_WON(("ConnectRoomCallback out"));
    }


    //
    // RegisterPlayerCallback
    //
    void RegisterPlayerCallback(const MINTCLIENT::RoutingServerClient::RegisterClientResult& result)
    {
        auto context = static_cast<ConnectRoomContext*>(result.context);
        ASSERT(context);

        LOG_WON(("ConnectRoomCallback -> Routing Server -> Register -> RegisterPlayerCallback ->"));
        LOG_WON(("RegisterPlayerCallback info->: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        if (context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
                // case WONMsg::StatusRouting_ClientAlreadyRegistered:
                //     // OutputError("Software error: already logged in");
                //     // ... fall through.
                // case WONMsg::StatusCommon_Success:
            case WONAPI::Error_Success:
            {
                // Wait for data mutex
                // LOG_WON(("RegisterPlayerCallback critRouting.Wait"));
                critRouting.Wait();

                // If there is a current server, leave it and delete it.
                if (currentRoutingServer)
                {
                    currentRoutingServer->Close();
                    delete currentRoutingServer;
                }

                // Move the next server to the current server
                currentRoutingServer = context->routingServer;

                // LOG_WON(("RegisterPlayerCallback critRouting.Signal"));
                critRouting.Signal();

                // LOG_WON(("RegisterPlayerCallback critData.Wait"));
                critData.Wait();

                // Clear the next server
                context->routingServer = nullptr;

                // We are now in a new room!

                // Remove all players from the list
                roomClients.DisposeAll();

                // Remove all games from the list
                roomGames.DisposeAll();

                // Signal for data mutex
                // LOG_WON(("RegisterPlayerCallback critData.Signal"));
                critData.Signal();

                // Begin player list update
                currentRoutingServer->GetUserList
                (
                    UserListCallback,
                    nullptr
                );

                // // Begin game list updates
                // currentRoutingServer->SubscribeDataObjectEx
                // (
                //     WONAPI::RoutingServerClient::GROUPID_ALLUSERS,  // ClientOrGroupId theLinkId
                //     dataGamePrefix,                                 // const WONCommon::RawBuffer& theDataTypeR
                //     FALSE,                                          // bool flagExactOrRecursive
                //     TRUE,                                           // bool flagGroupOrMembers
                //     SubscribeDataObjectCallback,                    // void (*f)(const ReadDataObjectResult&, privsType)
                //     (void*)NULL                                     // privsType t
                // );

                // Begin game list updates
                currentRoutingServer->GetGameList(
                    GameListCallback,
                    nullptr
                );

                PostEvent(Message::EnteredRoom, new Message::Data::EnteredRoom(context->roomName.str));
                PostEvent(Message::PlayersChanged);
                PostEvent(Message::GamesChanged);
                break;
            }

            // case WONMsg::StatusRouting_ClientAlreadyExists:
            //     PostEvent(Error::JoinRoomBadUsername);
            //     break;
            //
            // case WONMsg::StatusRouting_InvalidPassword:
            //     PostEvent(Error::JoinRoomBadPassword);
            //     break;
            //
            // case WONMsg::StatusRouting_ServerFull:
            //     PostEvent(Error::JoinRoomFull);
            //     break;

            default:
                LOG_ERR(("RegisterEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                PostEvent(Error::JoinRoomFailure);
                break;
            }
        }

        delete context;

        LOG_WON(("RegisterPlayerCallback out"));
    }


    //
    // UserListCallback
    //
    void UserListCallback(const MINTCLIENT::RoutingServerClient::GetUserListResult& result)
    {
        LOG_WON(("ClientListCallback in"));
        LOG_WON(("GetClientListEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
        {
            LOG_WON(("ClientListCallback critData.Wait"));
            critData.Wait();
            for (const auto& i : result.clientDataList)
            {
                roomClients.AddNoDup(i);
            }
            LOG_WON(("ClientListCallback critData.Signal"));
            critData.Signal();

            PostEvent(Message::PlayersChanged);
            break;
        }

        default:
            LOG_ERR(("GetClientListEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            break;
        }

        LOG_WON(("ClientListCallback out"));
    }


    //
    // PostEvent
    //
    void PostEvent(U32 message, void* data)
    {
        ASSERT(initialized);

        LOG_DEV(("Posting Event [%08x] %s", message, DecodeMessage(message)));

        Event* e = eventQueue.AddPre();
        ASSERT(e);
        e->message = message;
        e->data = data;
        eventQueue.AddPost();
    }


    //
    // DecodeMessage
    //
    const char* DecodeMessage(U32 message)
    {
        switch (message)
        {
        case Message::RetrievedServerList:
            return ("Message::RetrievedServerList");

        case Message::FirewallStatus:
            return ("Message::FirewallStatus");

        case Message::LoggedIn:
            return ("Message::LoggedIn");

        case Message::CreatedAccount:
            return ("Message::CreatedAccount");

        case Message::ChangedPassword:
            return ("Message::ChangedPassword");

        case Message::Chat:
            return ("Message::Chat");

        case Message::InitialRoomUpdate:
            return ("Message::InitialRoomUpdate");

        case Message::RoomsUpdated:
            return ("Message::RoomsUpdated");

        case Message::CreatedRoom:
            return ("Message::CreatedRoom");

        case Message::RegisteredRoom:
            return ("Message::RegisteredRoom");

        case Message::ConnectedRoom:
            return ("Message::ConnectedRoom");

        case Message::EnteredRoom:
            return ("Message::EnteredRoom");

        case Message::PlayersChanged:
            return ("Message::PlayersChanged");

        case Message::GamesChanged:
            return ("Message::GamesChanged");

        case Message::CreatedGame:
            return ("Message::CreatedGame");

            //
            // HTTP
            //

        case Message::HTTPProgressUpdate:
            return ("Message::HTTPProgressUpdate");

        case Message::HTTPCompleted:
            return ("Message::HTTPCompleted");

            //
            // HTTP
            //

        case Error::ConnectionFailure:
            return ("Error::ConnectionFailure");

        case Error::LoginInvalidUsername:
            return ("Error::LoginInvalidUsername");

        case Error::LoginInvalidPassword:
            return ("Error::LoginInvalidPassword");

        case Error::LoginFailure:
            return ("Error::LoginFailure");

        case Error::KeyExpired:
            return ("Error::KeyExpired");

        case Error::VerifyFailed:
            return ("Error::VerifyFailed");

        case Error::LockedOut:
            return ("Error::LockedOut");

        case Error::KeyInUse:
            return ("Error::KeyInUse");

        case Error::KeyInvalid:
            return ("Error::KeyInvalid");

        case Error::CreateAccountFailure:
            return ("Error::CreateAccountFailure");

        case Error::CreateAccountBadUser:
            return ("Error::CreateAccountBadUser");

        case Error::CreateAccountBadPassword:
            return ("Error::CreateAccountBadPassword");

        case Error::ChangePasswordFailure:
            return ("Error::ChangePasswordFailure");

        case Error::ChangePasswordBadNewPassword:
            return ("Error::ChangePasswordBadNewPassword");

        case Error::NoLobby:
            return ("Error::NoLobby");

        case Error::NoRoom:
            return ("Error::NoRoom");

        case Error::JoinRoomFailure:
            return ("Error::JoinRoomFailure");

        case Error::JoinRoomBadUsername:
            return ("Error::JoinRoomBadUsername");

        case Error::JoinRoomBadPassword:
            return ("Error::JoinRoomBadPassword");

        case Error::JoinRoomFull:
            return ("Error::JoinRoomFull");

        case Error::CreateRoomFailure:
            return ("Error::CreateRoomFailure");

        case Error::CreateGameFailure:
            return ("Error::CreateGameFailure");

        case Error::ReconnectFailure:
            return ("Error::ReconnectFailure");

        case Error::HTTPFailed:
            return ("Error::HTTPFailed");

        default:
            return ("Unknown!");
        }
    }


    //
    // Request the list of rooms
    //
    void UpdateRooms(AbortableContext* context)
    {
        LOG_WON(("$$$ void UpdateRooms"));

        if (!Utils::Strlen(identity.GetLoginName()))
        {
            ERR_FATAL(("No Identity!"));
        }

        LOG_WON(("GetDirectoryEx: Num Servers %d", directoryServers.servers.size()));

        // WONAPI::Error error = WONAPI::GetDirectoryEx
        // (
        //     &identity,                    // Identity* identity
        //     directoryServers.servers,     // const IPSocket::Address* directoryServers
        //     directoryServers.num,         // unsigned int numAddrs
        //     NULL,                         // IPSocket::Address* fromDirServer
        //     dirDarkReign2,                // const WONCommon::WONString& path
        //     NULL,                         // WONMsg::DirEntityList* result
        //     WONMsg::GF_DECOMPSERVICES |   // long flags
        //     WONMsg::GF_ADDTYPE |
        //     WONMsg::GF_ADDDISPLAYNAME |
        //     WONMsg::GF_SERVADDNAME |
        //     WONMsg::GF_SERVADDNETADDR |
        //     WONMsg::GF_ADDDOTYPE |
        //     WONMsg::GF_ADDDODATA,
        //     data,                         // const WONCommon::DataObjectTypeSet& dataSet
        //     0,                            // DirEntityCallback callback 
        //     0,                            // void* callbackPrivData
        //     requestTimeout,               // long timeout
        //     TRUE,                         // bool async
        //     UpdateRoomsCallback,          // void (*f)(const DirEntityListResult&, privsType)
        //     context                       // privsType privs
        // );

        // `GetDirectory` called with `identity` means we're logged in, let's look for items we have access to.
        WONAPI::Error error = MINTCLIENT::Directory::GetDirectory(
            &identity,
            &directoryServers.servers,
            UpdateRoomsCallback,
            context
        );

        switch (error)
        {
        case WONAPI::Error_Success:
        case WONAPI::Error_Pending:
            break;

        default:
            ERR_FATAL(("GetDirectoryEx: %d %s", error, WONAPI::WONErrorToString(error)));
        }
    }


    //
    // Request the list of rooms (without context)
    //
    void UpdateRooms()
    {
        UpdateRooms(nullptr);
    }


    //
    // UpdateRoomCallback
    //
    void UpdateRoomsCallback(const MINTCLIENT::Directory::Result& result)
    {
        LOG_WON(("UpdateRoomsCallback in"));
        LOG_WON(("DirectoryEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        auto context = static_cast<AbortableContext*>(result.context);

        if (context && context->abort.Test())
        {
            LOG_WON(("Was Aborted"));
        }
        else
        {
            switch (result.error)
            {
            case WONAPI::Error_Success:
            {
                U16 numFactory = 0;

                // WONMsg::DirEntityList::const_iterator i;
                //
                // // Iterate the server list and count the number of various server types
                // for(i = result.entityList->begin(); i != result.entityList->end(); ++i)
                // {
                //     if (i->mName == serverFactory)
                //     {
                //         numFactory++;
                //     }
                // }

                // Enter a data safe zone
                LOG_WON(("UpdateRoomsCallback critData.Wait"));
                critData.Wait();

                // Resize our server arrays to accomodate these new sizes
                chatServers.DisposeAll();
                factoryServers.Resize(numFactory);

                // Reset nums so that they can be used as indexes
                numFactory = 0;

                // Fill in the new servers
                for (auto i : result.room_list)
                {
                    if (Utils::Strcmp(i.type.str, (CH*)serverRouting) == 0)
                    {
                        ChatServer* server = new ChatServer(
                            i.name.str,
                            MINTCLIENT::IPSocket::Address(Utils::Unicode2Ansi(i.address.str)),
                            i.num_players,
                            i.password,
                            i.permanent
                        );

                        char buf[128];
                        Utils::Unicode2Ansi(buf, 128, server->name.str);

                        LOG_WON(("Routing Server: %s [%s] [%d] [%s]", buf, server->address.GetHost(), i.num_players, i.password ? "locked" : "unlocked"));
                        chatServers.Add(server->name.crc, server);
                    }
                    // else if (i.mName == serverFactory)
                    // {
                    //     factoryServers.servers[numFactory] = WONAPI::IPSocket::Address(*(unsigned long*)(i->mNetAddress.data() + 2), ntohs(*(unsigned short*)i->mNetAddress.data()));
                    //     // LOG_WON(("Factory Server: %s", factoryServers.servers[numFactory].GetAddressString(TRUE).c_str()));
                    //     numFactory++;
                    // }
                }

                // Leaving a data safe zone
                LOG_WON(("UpdateRoomsCallback critData.Signal"));
                critData.Signal();

                if (context)
                {
                    PostEvent(Message::InitialRoomUpdate);
                    JoinLobby(context);
                    context = nullptr;
                }
                else
                {
                    PostEvent(Message::RoomsUpdated);
                }
                break;
            }

            case WONAPI::Error_Timeout:
            {
                if (context)
                {
                    context->abort.Signal();
                }
            }
            break;

            default:
                LOG_ERR(("DirectoryEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
                break;
            }
        }

        // Delete `context` if available.
        delete context;

        LOG_WON(("UpdateRoomsCallback out"));
    }


    //
    // Join a lobby
    //
    void JoinLobby(AbortableContext* context)
    {
        ASSERT(context);
        LOG_WON(("Joining a Lobby"));

        if (!Utils::Strlen(identity.GetLoginName()))
        {
            ERR_FATAL(("No Identity!"));
        }

        ChatServer* lobby = NULL;

        // Wait for data mutex
        LOG_WON(("JoinLobby critData.Wait"));
        critData.Wait();

        // Look through the list of rooms for a suitable lobby with the following criteria:
        // - The `permanent=1` flag. (It's a server room, not a custom user room).
        // - Has the most players.
        // - If all lobbies are over the limit, pick the lobby with the least players.

        for (NBinTree<ChatServer>::Iterator c(&chatServers); *c; ++c)
        {
            if ((*c)->permanent)
            {
                if (lobby)
                {
                    if ((*c)->numPlayers < maxLobbyUsers)
                    {
                        if (lobby->numPlayers < (*c)->numPlayers || lobby->numPlayers >= maxLobbyUsers)
                        {
                            lobby = *c;
                        }
                    }
                    else
                    {
                        if (lobby->numPlayers >= maxLobbyUsers && lobby->numPlayers > (*c)->numPlayers)
                        {
                            lobby = *c;
                        }
                    }
                }
                else
                {
                    lobby = *c;
                }
            }
        }

        // Signal data mutex
        LOG_WON(("JoinLobby critData.Signal"));
        critData.Signal();

        if (lobby)
        {
            ConnectRoom(lobby->address, (CH*)lobby->name.str, (CH*)"", context);
        }
        else
        {
            PostEvent(Error::NoLobby);
            delete context;
        }
    }


    //
    // TitanServerListCallback
    //
    void TitanServerListCallback(const MINTCLIENT::Directory::Result& result)
    {
        auto context = static_cast<AbortableContext*>(result.context);
        ASSERT(context);

        LOG_WON(("ServerListCallback in"));
        LOG_WON(("GetDirectoryEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        if (context->abort.Wait(0))
        {
            LOG_WON(("Was Aborted"));
        }
        switch (result.error)
        {
        case WONAPI::Error_Success:
        {
            U16 numAuth = 0;
            U16 numFirewall = 0;
            U16 numProfile = 0;

            // Iterate the server list and count the number of various server types.
            for (auto i : result.server_list)
            {
                if (Utils::Strcmp(i.type.str, (CH*)serverAuth) == 0)
                {
                    numAuth++;
                }
                if (Utils::Strcmp(i.type.str, (CH*)serverFirewall) == 0)
                {
                    numFirewall++;
                }
                else if (Utils::Strcmp(i.type.str, (CH*)serverProfile) == 0)
                {
                    numProfile++;
                }
            }

            // Wait for data mutex
            LOG_WON(("TitanServerListCallback critData.Wait"));
            critData.Wait();

            // Resize our server arrays to accomodate these new sizes
            authServers.Resize(numAuth);
            firewallServers.Resize(numFirewall);
            profileServers.Resize(numProfile);

            // Reset nums so that they can be used as indexes
            numAuth = 0;
            numFirewall = 0;
            numProfile = 0;

            // Fill in the new servers
            for (auto i : result.server_list)
            {
                if (Utils::Strcmp(i.type.str, (CH*)serverAuth) == 0)
                {
                    authServers.servers[numAuth] = MINTCLIENT::IPSocket::Address(Utils::Unicode2Ansi(i.address.str));
                    LOG_WON(("Authentication Server: %s", authServers.servers[numAuth].GetText()));
                    numAuth++;
                }
                else if (Utils::Strcmp(i.type.str, (CH*)serverFirewall) == 0)
                {
                    firewallServers.servers[numFirewall] = MINTCLIENT::IPSocket::Address(Utils::Unicode2Ansi(i.address.str));
                    LOG_WON(("Firewall Server: %s", firewallServers.servers[numFirewall].GetText()));
                    numFirewall++;
                }
                else if (Utils::Strcmp(i.type.str, (CH*)serverProfile) == 0)
                {
                    //profileServers.servers[numProfile] = WONAPI::IPSocket::Address(*(unsigned long*)(i->mNetAddress.data() + 2), ntohs(*(unsigned short*)i->mNetAddress.data()));
                    //LOG_WON(("Profile Server: %s", profileServers.servers[numProfile].GetAddressString(TRUE).c_str()));
                    numProfile++;
                }
            }

            // Signal data mutex
            LOG_WON(("TitanServerListCallback critData.Signal"));
            critData.Signal();

            // If there are firewall servers detect to see if we're behind a firewall
            if (firewallServers.servers.size() && firewallStatus == Firewall::Unchecked)
            {
                // Detect firewall
                DetectFirewall();
            }

            // Are there any authentication servers
            if (authServers.Length())
            {
                // We are now connected:
                // - The login screen appears for the next workflow to commence.
                connected = TRUE;
                PostEvent(Message::RetrievedServerList);
            }
            else
            {
                // No auth servers could be found
                PostEvent(Error::ConnectionFailure);
            }
        }
        break;

        case WONAPI::Error_Timeout:
            PostEvent(Error::ConnectionFailure);
            break;

        default:
            LOG_ERR(("GetDirectoryEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            PostEvent(Error::ConnectionFailure);
            break;
        }

        delete context;

        LOG_WON(("ServerListCallback out"));
    }


    //
    // HTTPGetCallback
    //
    void HTTPGetCallback(WONAPI::Error result, HTTPdata* data)
    {
        // LOG_WON(("HTTPGetCallback in"));
        //
        // switch (result)
        // {
        // case WONMsg::StatusCommon_Success:
        // {
        //     // The file was downloaded successfully
        //     PostEvent(Message::HTTPCompleted, new Message::Data::HTTPCompleted(data->handle, data->isNew));
        //     break;
        // }
        //
        // default:
        // {
        //     LOG_ERR(("HTTPGetCallback CB: %d %s", result, WONErrorToString(result)))
        //         PostEvent(Error::HTTPFailed, new Error::Data::HTTPFailed(data->handle));
        //     break;
        // }
        // }
        //
        // LOG_WON(("httpDataCritSect.Wait"));
        // httpDataCritSect.Wait();
        // httpData.Dispose(data);
        // LOG_WON(("httpDataCritSect.Signal"));
        // httpDataCritSect.Signal();
        //
        // LOG_WON(("HTTPGetCallback out"));
    }


    //
    // HTTPProgressCallback
    //
    bool HTTPProgressCallback(unsigned long progress, unsigned long size, void* callbackPrivData)
    {
        return TRUE;

        // HTTPdata* data = static_cast<HTTPdata*>(callbackPrivData);
        //
        // LOG_WON(("httpDataCritSect.Wait"));
        // httpDataCritSect.Wait();
        // Bool abort = data->abort;
        // LOG_WON(("httpDataCritSect.Signal"));
        // httpDataCritSect.Signal();
        //
        // // If this updates for every byte we'll probably need to throttle it back a tad
        // PostEvent(Message::HTTPProgressUpdate, new Message::Data::HTTPProgressUpdate(data->handle, progress, size));
        //
        // if (abort)
        // {
        //     LOG_WON(("HTTPProgressCallback: aborting download"));
        // }
        //
        // return (abort ? FALSE : TRUE);
    }


    //
    // CreateGameCallback
    //
    void CreateGameCallback(const MINTCLIENT::RoutingServerClient::CreateGameResult& result)
    {
        LOG_WON(("CreateGameCallback in"));
        LOG_WON(("CreateGameCallback CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
            PostEvent(Message::CreatedGame);
            break;

        case WONAPI::Error_AddressInUse:
            PostEvent(Error::CreateGameFailure);
            break;

        default:
            LOG_WON(("!!!Unhandled Result!!! CreateGameCallback: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            break;
        }

        LOG_WON(("CreateGameCallback out"));
    }


    //
    // DeleteGameCallback
    //
    void DeleteGameCallback(const MINTCLIENT::RoutingServerClient::DeleteGameResult& result)
    {
        LOG_WON(("DeleteDataObjectCallback in"));
        LOG_WON(("DeleteDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
            // Game has been deleted.
            break;

        default:
            LOG_ERR(("DeleteDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            break;
        }

        LOG_WON(("DeleteDataObjectCallback out"));
    }


    //
    // UpdateGameCallback
    //
    void UpdateGameCallback(const MINTCLIENT::RoutingServerClient::UpdateGameResult& result)
    {
        LOG_WON(("ReplaceDataObjectCallback in"));
        LOG_WON(("ReplaceDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
            // Game has been replaced.
            break;

        default:
            LOG_ERR(("ReplaceDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            break;
        }

        LOG_WON(("ReplaceDataObjectCallback out"));
    }


    //
    // GetGameListCallback
    //
    void GameListCallback(const MINTCLIENT::RoutingServerClient::GetGameListResult& result)
    {
        LOG_WON(("SubscribeDataObjectCallback in"));

        LOG_WON(("SubscribeDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));

        switch (result.error)
        {
        case WONAPI::Error_Success:
        {
            LOG_WON(("Subscribed to game list"));

            LOG_WON(("critData.Wait"));
            critData.Wait();

            for (const auto& i : result.gameResultList)
            {
                GameName name = Utils::Unicode2Ansi(Utils::Strdup(i.name.str));

                RoomClient* host = roomClients.Find(i.ownerId);

                if (host)
                {
                    RoomGame* game = new RoomGame
                    (
                        name,
                        host->name,
                        i.game_data_size,
                        i.game_data
                    );
                    roomGames.Add(name.crc, game);
                    LOG_WON(("Game: %s", name.str));

                    RoomName gameName;
                    Utils::Ansi2Unicode(gameName.str, gameName.GetSize(), name.str);

                    PlayerName name = host->name.str;

                    LOG_WON(("critData.Signal"));
                    critData.Signal();
                    PostEvent(Message::Chat, new Message::Data::Chat(Message::Data::Chat::GameCreated, gameName.str, name.str));
                    PostEvent(Message::GamesChanged);
                    LOG_WON(("critData.Wait"));
                    critData.Wait();
                }
            }

            LOG_WON(("critData.Signal"));
            critData.Signal();

            break;
        }

        default:
            LOG_ERR(("SubscribeDataObjectEx CB: %d %s", result.error, WONAPI::WONErrorToString(result.error)));
            break;
        }

        LOG_WON(("SubscribeDataObjectCallback out"));
    }


    //
    // GetNumUsersCallback
    //
    void GetNumUsersCallback(const MINTCLIENT::RoutingServerClient::GetNumUsersResult& result)
    {
        if (result.error != WONAPI::Error_Success)
        {
            PostEvent(Error::ReconnectFailure);
        }
    }


    //
    // ClientJoinAttemptCatcher
    //
    // void ClientJoinAttemptCatcher(const MINTCLIENT::RoutingServerClient::Data::ClientDataWithReason& reason, void*)
    // {
    //     LOG_WON(("ClientJoinAttempt"));
    //     reason;
    // }


    //
    // ClientEnterExCatcher
    //
    void ClientEnterCatcher(const MINTCLIENT::RoutingServerClient::ClientEnterResult& result)
    {
        RoomClient* newClient;

        LOG_WON(("critData.Wait"));
        critData.Wait();

        if ((newClient = roomClients.AddNoDup(result.client)) != nullptr)
        {
            LOG_WON(("critData.Signal"));
            critData.Signal();

            // Notify interface of change of players
            PostEvent(Message::PlayersChanged);

            // Print a message with this player's name
            PostEvent(Message::Chat, new Message::Data::Chat(Message::Data::Chat::PlayerEntered, (const CH*)nullptr, newClient->name.str));
        }
        else
        {
            LOG_WON(("critData.Signal"));
            critData.Signal();
        }
    }


    //
    // ClientLeaveCatcher
    //
    void ClientLeaveCatcher(const MINTCLIENT::RoutingServerClient::ClientLeaveResult& result)
    {
        PlayerName name;

        LOG_WON(("critData.Wait"));
        critData.Wait();

        if (roomClients.Remove(result.client.clientId, name))
        {
            LOG_WON(("critData.Signal"));
            critData.Signal();

            // Notify interface of change of players
            PostEvent(Message::PlayersChanged);

            // Print a message with this player's name
            PostEvent(Message::Chat, new Message::Data::Chat(Message::Data::Chat::PlayerLeft, (const CH*)nullptr, name.str));
        }
        else
        {
            LOG_WON(("critData.Signal"));
            critData.Signal();
        }
    }


    //
    // MuteClientCatcher
    //
    void MuteClientCatcher(const MINTCLIENT::RoutingServerClient::ClientUpdateResult& result)
    {
        LOG_WON(("MuteClient [%d] %d", result.client.clientId, result.client.isMuted));
        
        LOG_WON(("critData.Wait"));
        critData.Wait();
        RoomClient* rc = roomClients.Find(result.client.clientId);
        if (rc)
        {
            rc->muted = result.client.isMuted;
        }
        LOG_WON(("critData.Signal"));
        critData.Signal();
        
        PostEvent(Message::PlayersChanged);
    }


    //
    // BecomeModeratorCatcher
    //
    void BecomeModeratorCatcher(const MINTCLIENT::RoutingServerClient::ClientUpdateResult& result)
    {
        LOG_WON(("BecomeModerator [%d] %d", result.client.clientId, result.client.isModerator));
        
        LOG_WON(("critData.Wait"));
        critData.Wait();
        RoomClient* rc = roomClients.Find(result.client.clientId);
        if (rc)
        {
            rc->moderator = result.client.isModerator;
        }
        LOG_WON(("critData.Signal"));
        critData.Signal();
        
        PostEvent(Message::PlayersChanged);
    }


    //
    // HostChangeCatcher
    //
    void HostChangeCatcher(const MINTCLIENT::RoutingServerClient::ClientUpdateResult& result)
    {
        LOG_WON(("HostChange"));
        result;
    }


    //
    // A StyxNet Session Game has been created.
    //
    void GameCreatedCatcher(const MINTCLIENT::RoutingServerClient::CreateGameResult& result)
    {
        LOG_WON(("GameCreatedCatcher in"));
        GameName name = Utils::Strdup(Utils::Unicode2Ansi(result.name.str)); // reinterpret_cast<const char*>(reason.mDataType.data() + dataGamePrefix.size());
        LOG_WON(("Game Created : `%s` by [%d]", name.str, result.ownerId));

        LOG_WON(("critData.Wait"));
        critData.Wait();

        RoomClient* host = roomClients.Find(result.ownerId);

        if (host)
        {
            RoomName gameName;
            Utils::Ansi2Unicode(gameName.str, gameName.GetSize(), name.str);

            auto game_data_size = result.game_data_size;
            auto game_data = result.game_data;

            auto session = (StyxNet::Session*)result.game_data;
            auto addr_text = session->address.GetText();
            auto addr_info = session->address.GetDisplayText();

            LOG_WON((">>> Found host : `%s` at address `%s`", Utils::Unicode2Ansi(host->name.str), addr_info));

            session->address.SetIP(result.address.host);
            LOG_WON((">>> Switching IP from `%s` to `%s`", session->address.GetDisplayText(), result.address.host));

            RoomGame* game = new RoomGame
            (
                name.str,
                host->name,
                game_data_size,
                game_data
            );

            roomGames.Add(game->name.crc, game);

            PlayerName name = host->name.str;

            LOG_WON(("critData.Signal"));
            critData.Signal();
            PostEvent(Message::Chat, new Message::Data::Chat(Message::Data::Chat::GameCreated, gameName.str, name.str));
            PostEvent(Message::GamesChanged);
            LOG_WON(("critData.Wait"));
            critData.Wait();
        }
        else
        {
            LOG_WON(("Couldn't resolve the host!"));
        }
        LOG_WON(("critData.Signal"));
        critData.Signal();

        LOG_WON(("DataObjectCreationCatcher out"));
    }


    //
    // A StyxNet Session Game has been updated.
    //
    void GameUpdatedCatcher(const MINTCLIENT::RoutingServerClient::UpdateGameResult& result)
    {
        LOG_WON(("GameUpdatedCatcher in"));
        GameName name = Utils::Strdup(Utils::Unicode2Ansi(result.name.str)); // reinterpret_cast<const char*>(reason.mDataType.data() + dataGamePrefix.size());
        LOG_WON(("Game Modified: %s", name.str));
        LOG_WON(("GameUpdatedCatcher out"));
    }


    //
    // A StyxNet Session Game has been replaced.
    //
    void GameReplacedCatcher(const MINTCLIENT::RoutingServerClient::ReplaceGameResult& result)
    {
        LOG_WON(("GameReplacedCatcher in"));
        GameName name = Utils::Strdup(Utils::Unicode2Ansi(result.name.str)); // reinterpret_cast<const char*>(reason.mDataType.data() + dataGamePrefix.size());
        LOG_WON(("Game Updated : %s by [%d]", name.str, result.ownerId));

        LOG_WON(("critData.Wait"));
        critData.Wait();

        RoomGame* game = roomGames.Find(name.crc);
        if (game)
        {
            if (game->size == result.game_data_size)
            {
                Utils::Memcpy(game->data, result.game_data, game->size);
                LOG_WON(("critData.Signal"));
                critData.Signal();
                PostEvent(Message::GamesChanged);
                LOG_WON(("critData.Wait"));
                critData.Wait();
            }
            else
            {
                LOG_WON(("Data is a different size"));
            }
        }
        else
        {
            LOG_WON(("Could not find game"));
        }

        LOG_WON(("critData.Signal"));
        critData.Signal();

        LOG_WON(("GameReplacedCatcher out"));
    }


    //
    // A StyxNet Session Game has been deleted.
    //
    void GameDeletedCatcher(const MINTCLIENT::RoutingServerClient::DeleteGameResult& result)
    {
        LOG_WON(("GameDeletedCatcher in"));
        GameName name = Utils::Strdup(Utils::Unicode2Ansi(result.name.str)); // reinterpret_cast<const char*>(reason.mDataType.data() + dataGamePrefix.size());
        LOG_WON(("Game Deleted : %s by [%d]", name.str, result.ownerId));

        LOG_WON(("critData.Wait"));
        critData.Wait();

        RoomGame* game = roomGames.Find(name.crc);
        if (game)
        {
            roomGames.Dispose(game);

            RoomName gameName;
            Utils::Ansi2Unicode(gameName.str, gameName.GetSize(), name.str);

            LOG_WON(("critData.Signal"));
            critData.Signal();
            PostEvent(Message::Chat, new Message::Data::Chat(Message::Data::Chat::GameDestroyed, gameName.str, (const CH*)NULL));
            PostEvent(Message::GamesChanged);
        }
        else
        {
            LOG_WON(("critData.Signal"));
            critData.Signal();
        }

        LOG_WON(("GameDeletedCatcher out"));
    }


    //
    // KeepAliveCatcher
    //
    void KeepAliveCatcher(void*)
    {
        LOG_WON(("KeepAlive"));
    }


    //
    // RawPeerChatCatcher
    //
    void RawPeerChatCatcher(const MINTCLIENT::RoutingServerClient::Data::RawChatMessage& reason, void*)
    {
        LOG_WON(("RawPeerChat"));
        reason;
    }


    //
    // ASCIIPeerChatCatcher
    //
    void ASCIIPeerChatCatcher(const MINTCLIENT::RoutingServerClient::ASCIIChatMessageResult& result)
    {
        LOG_WON(("ASCIIPeerChatCatcher critData.Wait"));
        critData.Wait();

        RoomClient* from = roomClients.Find(result.chatMessage.clientId);

        if (from)
        {
            // Are we ignoring messages from this player?
            if (ignoredPlayers.Exists(from->name.crc))
            {
                LOG_WON(("ASCIIPeerChatCatcher critData.Signal"));
                critData.Signal();
            }
            else
            {
                U32 type = Message::Data::Chat::Broadcast;

                switch (result.chatMessage.type)
                {
                case MINTCLIENT::RoutingServerClient::Data::ASCIIChatMessage::CHATTYPE_ASCII_EMOTE:
                    type = Message::Data::Chat::Emote;
                    break;

                case MINTCLIENT::RoutingServerClient::Data::ASCIIChatMessage::CHATTYPE_ASCII:
                    type = Message::Data::Chat::Broadcast;
                    break;
                }

                if (result.chatMessage.isWhisper)
                {
                    type = Message::Data::Chat::Private;
                }

                PlayerName name = from->name.str;

                LOG_WON(("ASCIIPeerChatCatcher critData.Signal"));
                critData.Signal();
                PostEvent(Message::Chat, new Message::Data::Chat(type, result.chatMessage.text.str, name.str));
            }
        }
        else
        {
            LOG_WON(("ASCIIPeerChatCatcher critData.Signal"));
            critData.Signal();
        }
    }


    //
    // UnicodePeerChatCatcher
    //
    void UnicodePeerChatCatcher(const MINTCLIENT::RoutingServerClient::Data::UnicodeChatMessage& message, void*)
    {
        // LOG_WON(("critData.Wait"));
        // critData.Wait();
        //
        // RoomClient* from = roomClients.Find(message.mSenderId);
        //
        // if (from)
        // {
        //     // Are we ignoring messages from this player ?
        //     if (ignoredPlayers.Exists(from->name.crc))
        //     {
        //         LOG_WON(("critData.Signal"));
        //         critData.Signal();
        //     }
        //     else
        //     {
        //         U32 type = Message::Data::Chat::Broadcast;
        //
        //         switch (message.mChatType)
        //         {
        //         case CHATTYPE_UNICODE_EMOTE:
        //             type = Message::Data::Chat::Emote;
        //             break;
        //
        //         case CHATTYPE_UNICODE:
        //             type = Message::Data::Chat::Broadcast;
        //             break;
        //         }
        //
        //         if (message.IsWhisper())
        //         {
        //             type = Message::Data::Chat::Private;
        //         }
        //
        //         PlayerName name = from->name.str;
        //
        //         LOG_WON(("critData.Signal"));
        //         critData.Signal();
        //         PostEvent(Message::Chat, new Message::Data::Chat(type, message.mData.c_str(), name.str));
        //     }
        // }
        // else
        // {
        //     LOG_WON(("critData.Signal"));
        //     critData.Signal();
        // }
    }


    //
    // ReconnectFailureCatcher
    //
    void ReconnectFailureCatcher(void*)
    {
        LOG_WON(("ReconnectFailure"));

        LOG_WON(("critRouting.Wait"));
        critRouting.Wait();
        if (currentRoutingServer)
        {
            delete currentRoutingServer;
            currentRoutingServer = nullptr;
        }
        LOG_WON(("critRouting.Signal"));
        critRouting.Signal();

        PostEvent(Error::ReconnectFailure);
    }


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Struct AbortableContext
    //

    //
    // Constructor
    //
    AbortableContext::AbortableContext()
    {
        critAbortable.Wait();
        abortableContexts.Append(this);
        critAbortable.Signal();
    }


    //
    // Destructor
    //
    AbortableContext::~AbortableContext()
    {
        critAbortable.Wait();
        abortableContexts.Unlink(this);
        critAbortable.Signal();
    }


    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Message
    //
    namespace Message
    {
        ///////////////////////////////////////////////////////////////////////////
        //
        // Struct Chat
        //
        namespace Data
        {
            //
            // Duplicate a string using uncached memory
            //
            static CH* SafeDup(const CH* str, U32 maxLen = 128)
            {
                U32 len = Min<U32>(Utils::Strlen(str), maxLen);
                U32 size = (len + 1) * 2;

                CH* text = reinterpret_cast<CH*>(Debug::Memory::UnCached::Alloc(size));
                Utils::Strmcpy(text, str, len + 1);
                text[len] = 0;

                return (text);
            }


            //
            // Duplicate an ANSI string using uncached memory and convert to Unicode
            //
            static CH* SafeDup(const char* str, U32 maxLen = 128)
            {
                U32 len = Min<U32>(Utils::Strlen(str), maxLen);
                U32 size = (len + 1) * 2;

                CH* text = reinterpret_cast<CH*>(Debug::Memory::UnCached::Alloc(size));
                Utils::Ansi2Unicode(text, size, str, len);
                text[len] = 0;

                return (text);
            }


            /////////////////////////////////////////////////////////////////////////
            //
            // Struct Chat
            //

            const U32 MAX_CHAT_STR = 200;

            //
            // Constructor
            //
            Chat::Chat(U32 id, const CH* textIn, const CH* userIn)
                : id(id),
                text(nullptr),
                user(nullptr)
            {
                if (textIn)
                {
                    text = SafeDup(textIn, MAX_CHAT_STR);
                }

                if (userIn)
                {
                    user = SafeDup(userIn);
                }
            }


            //
            // Constructor
            //
            Chat::Chat(U32 id, const char* textIn, const CH* userIn)
                : id(id),
                text(nullptr),
                user(nullptr)
            {
                if (textIn)
                {
                    text = SafeDup(textIn, MAX_CHAT_STR);
                }

                if (userIn)
                {
                    user = SafeDup(userIn);
                }
            }


            //
            // Destructor
            //
            Chat::~Chat()
            {
                if (text)
                {
                    Free(text);
                }
                if (user)
                {
                    Free(user);
                }
            }


            /////////////////////////////////////////////////////////////////////////
            //
            // Struct EnteredRoom
            //


            //
            // Constructor
            //
            EnteredRoom::EnteredRoom(const CH* text)
                : text(SafeDup(text))
            {
            }


            //
            // Destructor
            //
            EnteredRoom::~EnteredRoom()
            {
                Free(text);
            }
        }
    }
}
