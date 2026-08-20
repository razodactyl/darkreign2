///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// MultiPlayer Stuff
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "multiplayer_download.h"
#include "multiplayer_private.h"
#include "multiplayer_settings.h"
#include "stdload.h"
#include "ptree.h"
#include "woniface.h"
#include "console.h"
#include "iface.h"
#include "main.h"
#include "version.h"
#include "sha256.h"
#include "win32_socket.h"
#include "win32_dns.h"


///////////////////////////////////////////////////////////////////////////////
//
// Definitions
//
#define CAST(type, var, value) type var = reinterpret_cast<type>(value);


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace MultiPlayer
//
namespace MultiPlayer
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Namespace Download
    //
    namespace Download
    {
        ///////////////////////////////////////////////////////////////////////////////
        //
        // Struct Patch
        //
        struct Patch
        {
            // Code version
            U32 versionCode;

            // Data version
            U32 versionData;

            // Language
            U32 language;

            // Size
            U32 size;

            // Name of the file
            FilePath file;

            // SHA256 of the file as lowercase hex. Empty when the manifest did
            // not carry one, which is treated as a refusal to run it - see
            // VerifyPatch
            StrBuf<SHA256::STRING_SIZE> hash;

            // List node
            NList<Patch>::Node node;

            // Constructor
            Patch(FScope* fScope)
            {
                versionCode = StdLoad::TypeU32(fScope);
                versionData = StdLoad::TypeU32(fScope);
                language = StdLoad::TypeStringCrc(fScope, 0xE493D172); // "English"
                size = StdLoad::TypeU32(fScope, "Size");
                file = StdLoad::TypeString(fScope, "File");

                // Optional in the grammar so that a manifest stays readable by
                // older clients, mandatory in practice - see VerifyPatch
                hash = StdLoad::TypeString(fScope, "Hash", "");
            }
        };


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Internal Data
        //

        static Bool initialized = FALSE;

        static const char* fileVersion = "library\\engine\\version.cfg";
        static const char* fileDownload = "library\\engine\\download.cfg";

        // Our version
        static U32 language;
        static U32 versionCode;
        static U32 versionData;

        // Source of updates and motd information
        static HostName defaultHost;
        static U16 defaultPort;
        static FilePath defaultPath;

        // Where to look if the default source has gone away for good. Optional
        // in download.cfg; when present it is tried once, after the default
        // fails, before the check is reported as failed
        static Bool haveFallback;
        static HostName fallbackHost;
        static U16 fallbackPort;
        static FilePath fallbackPath;

        // Set while the current attempt is against the fallback, so that a
        // second failure reports rather than looping between the two
        static Bool usingFallback;

        // Name of the file for updates and motd
        static FileName fileUpdates;
        static FileName fileMotd;

        // Source of update downloads
        static HostName updateHost;
        static U16 updatePort;
        static FilePath updatePath;

        static U32 updateVersionCode;
        static U32 updateVersionData;

        // Download contexts
        static Context motdContext;
        static Context downloadContext;

        // Patches
        static NList<Patch> patches(&Patch::node);
        static Patch* patch;

        // Extras
        static NList<Extra> extras(&Extra::node);


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Prototypes
        //
        void Download(Context& context);
        void GetPatch(const Patch& patch);
        void Get(U32 type, const char* host, U16 port, const char* path, const char* file);
        void DNSCallback(const Win32::DNS::Host* host, void* context);
        Bool VerifyPatch(const Patch& patch);
        Bool RetryOnFallback();


        //
        // Initialization
        //
        void Init()
        {
            ASSERT(!initialized);

            versionCode = Version::GetBuildNumber();
            language = Crc::CalcStr(MultiLanguage::GetLanguage());

            // Load the version config file
            PTree pTreeVersion;
            if (pTreeVersion.AddFile(fileVersion))
            {
                // Get the global scope
                FScope* fScope = pTreeVersion.GetGlobalScope();

                versionData = StdLoad::TypeU32(fScope, "Data");
            }
            else
            {
                versionData = 0;
            }

            // Load the download config file
            PTree pTreeDownload;
            if (pTreeDownload.AddFile(fileDownload))
            {
                // Get the global scope
                FScope* fScope = pTreeDownload.GetGlobalScope();

                FScope* sScope = fScope->GetFunction("Source", TRUE);
                defaultHost = StdLoad::TypeString(sScope);
                defaultPort = U16(StdLoad::TypeU32(sScope, Range<U32>(0, U16_MAX)));
                defaultPath = StdLoad::TypeString(sScope);

                // A second home to fall back on, for the day the first one is
                // switched off. Optional - a config without one simply fails
                // the check as it always did
                if (FScope* fbScope = fScope->GetFunction("Fallback", FALSE))
                {
                    fallbackHost = StdLoad::TypeString(fbScope);
                    fallbackPort = U16(StdLoad::TypeU32(fbScope, Range<U32>(0, U16_MAX)));
                    fallbackPath = StdLoad::TypeString(fbScope);
                    haveFallback = TRUE;

                    LOG_DIAG(("Update fallback source is '%s:%d%s'", fallbackHost.str, fallbackPort, fallbackPath.str));
                }
                else
                {
                    haveFallback = FALSE;
                }

                usingFallback = FALSE;

                // Default the update source to the default source
                updateHost = defaultHost.str;
                updatePort = defaultPort;
                updatePath = defaultPath.str;

                fileUpdates = StdLoad::TypeString(fScope, "FileUpdates");
                fileMotd = StdLoad::TypeString(fScope, "FileMotd");
            }
            else
            {
                ERR_FATAL(("Could not load %s", fileDownload));
            }

            initialized = TRUE;
        }


        //
        // Shutdown
        //
        void Done()
        {
            ASSERT(initialized);

            // Delete patches and extras
            patches.DisposeAll();
            extras.DisposeAll();

            initialized = FALSE;
        }


        //
        // Abort
        //
        void Abort()
        {
            LOG_DIAG(("Aborting"));
            AbortByNameCallback(DNSCallback);
            AbortDownload();
        }


        //
        // Download the latest update file
        //
        void GetUpdates()
        {
            // Start from a clean slate. Without this a second check appends the
            // whole manifest to the lists again, and keeps the patch chosen the
            // first time round
            patches.DisposeAll();
            extras.DisposeAll();
            patch = nullptr;

            updateHost = defaultHost.str;
            updatePort = defaultPort;
            updatePath = defaultPath.str;
            usingFallback = FALSE;

            Get
            (
                0x325DC801, // "Updates"
                defaultHost.str,
                defaultPort,
                defaultPath.str,
                fileUpdates.str
            );
        }


        //
        // Download the message of the day
        //
        void GetMotd()
        {
            if (!motdContext.handle)
            {
                motdContext.Reset();
                motdContext.name = defaultHost.str;
                motdContext.host = defaultHost.str;
                motdContext.port = defaultPort;
                motdContext.path = defaultPath.str;
                motdContext.file = fileMotd.str;

                Download(motdContext);

                LOG_DIAG(("Downloading message of the day [%d]", motdContext.handle));
            }
        }


        //
        // Check the version
        //
        Bool CheckVersion()
        {
            return (TRUE);
        }


        //
        // Get the list of extras
        //
        const NList<Extra>& GetExtras()
        {
            return (extras);
        }


        //
        // Abort download
        //
        void AbortDownload()
        {
            LOG_DIAG(("Aborting Downloads"));
            downloadContext.aborted = TRUE;
            if (downloadContext.handle)
            {
                WonIface::HTTPAbortGet(downloadContext.handle);
            }
        }


        //
        // Get the download context
        //
        const Context& GetDownloadContext()
        {
            return (downloadContext);
        }


        //
        // Message
        //
        void Message(U32 message, void* data)
        {
            switch (message)
            {
                case WonIface::Message::HTTPProgressUpdate:
                {
                    ASSERT(data);
                    CAST(WonIface::Message::Data::HTTPProgressUpdate*, progress, data);

                    // LOG_DIAG(("Progress Update [%d] : %d of %d", progress->handle, progress->progress, progress->size))

                    if (progress->handle == downloadContext.handle)
                    {
                        downloadContext.size = progress->size;
                        downloadContext.transferred = progress->progress;
                    }
                    else
                    {
                        motdContext.size = progress->size;
                        motdContext.transferred = progress->progress;
                    }

                    delete progress;
                    break;
                }

                case WonIface::Message::HTTPCompleted:
                {
                    ASSERT(data);
                    CAST(WonIface::Message::Data::HTTPCompleted*, completed, data);

                    if (completed->handle == downloadContext.handle)
                    {
                        // Clear handle
                        downloadContext.handle = 0;

                        switch (downloadContext.type)
                        {
                            case 0x325DC801: // "Updates"
                            {
                                LOG_DIAG(("Downloaded updates file"));

                                PTree pTree;
                                if (pTree.AddFile(fileUpdates.str))
                                {
                                    // Get the global scope
                                    FScope* fScope = pTree.GetGlobalScope();

                                    while (FScope* sScope = fScope->NextFunction())
                                    {
                                        switch (sScope->NameCrc())
                                        {
                                            case 0x5FA3D48D: // "DefaultSource"
                                                updateHost = StdLoad::TypeString(sScope);
                                                updatePort = U16(StdLoad::TypeU32(sScope, Range<U32>(0, U16_MAX)));
                                                updatePath = StdLoad::TypeString(sScope);
                                                break;

                                            case 0x3D42B5CF: // "CurrentVersion"
                                                updateVersionCode = StdLoad::TypeU32(sScope);
                                                updateVersionData = StdLoad::TypeU32(sScope);
                                                break;

                                            case 0x1770E157: // "Patch"
                                                patches.Append(new Patch(sScope));
                                                break;

                                            case 0xCF498E8B: // "Extra"
                                                extras.Append(new Extra(sScope));
                                                break;
                                        }
                                    }

                                    // Compare our version to the version in the update
                                    if (versionCode < updateVersionCode || versionData < updateVersionData)
                                    {
                                        // We have an old version, is there a patch that will work for our version ?
                                        for (NList<Patch>::Iterator p(&patches); *p; ++p)
                                        {
                                            if ((*p)->language == language && (*p)->versionCode <= versionCode && (*p)->
                                                versionData <= versionData && (!patch || ((*p)->versionCode > patch->
                                                    versionCode && (*p)->versionData > patch->versionData)))
                                            {
                                                patch = *p;
                                            }
                                        }

                                        if (patch)
                                        {
                                            LOG_DIAG
                                            (
                                                ("Patching: current %d.%d patch for %d.%d patch to %d.%d", versionCode,
                                                    versionData, updateVersionCode, updateVersionData, patch->
                                                    versionCode, patch->versionData)
                                            );

                                            GetPatch(*patch);

                                            if (PrivData::updateCtrl.Alive())
                                            {
                                                // Tell 'em we're getting the patch
                                                SendEvent
                                                (
                                                    PrivData::updateCtrl, nullptr, IFace::NOTIFY,
                                                    0x96B48B0D
                                                ); // "Update::PatchAvailable"
                                            }
                                        }
                                        else
                                        {
                                            if (PrivData::updateCtrl.Alive())
                                            {
                                                // We are unpatchable
                                                SendEvent
                                                (
                                                    PrivData::updateCtrl, nullptr, IFace::NOTIFY,
                                                    0xF9068335
                                                ); // "Update::Unpatchable"
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (PrivData::updateCtrl.Alive())
                                        {
                                            // There is no patch required
                                            SendEvent
                                            (
                                                PrivData::updateCtrl, nullptr, IFace::NOTIFY,
                                                0xCA4DB1B4
                                            ); // "Update::NoPatch"
                                        }
                                    }
                                }
                                break;
                            }

                            case 0x1770E157: // "Patch"
                            {
                                // We downloaded a patch
                                ASSERT(patch);

                                // What we just downloaded is about to be run
                                // elevated, so it does not become the next
                                // process until it hashes to what the manifest
                                // said it would
                                if (!VerifyPatch(*patch))
                                {
                                    // Reported as an ordinary patch failure so
                                    // that the existing interface reacts to it;
                                    // the log says which it was
                                    SendEvent
                                    (
                                        PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                        0xB2623264
                                    ); // "Download::PatchFailed"
                                    break;
                                }

                                // Set the game to run that the patch next
                                Main::RegisterNextProcess(patch->file.str);

                                // Patch completed
                                SendEvent
                                (
                                    PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                    0x37976FA8
                                ); // "Download::PatchCompleted"
                                break;
                            }

                            default:
                                // Download completed
                                SendEvent
                                (
                                    PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                    0x7091B101
                                ); // "Download::Completed"
                                break;
                        }
                    }
                    else
                    {
                        LOG_DIAG(("Downloaded message of the day"));

                        // Display the motd to the console as a WonMessage
                        PTree pTree;
                        if (pTree.AddFile(fileMotd.str))
                        {
                            // Get the global scope
                            FScope* fScope = pTree.GetGlobalScope();

                            while (FScope* sScope = fScope->NextFunction())
                            {
                                switch (sScope->NameCrc())
                                {
                                    case 0xCB28D32D: // "Text"
                                    CONSOLE(0x70F02901, (StdLoad::TypeString(sScope))); // "MessageOfTheDay"
                                        break;
                                }
                            }
                        }
                    }

                    delete completed;
                    break;
                }

                case WonIface::Error::HTTPFailed:
                {
                    ASSERT(data);
                    CAST(WonIface::Error::Data::HTTPFailed*, failed, data);

                    if (failed->handle == downloadContext.handle)
                    {
                        // Clear the handle so a retry is able to start
                        downloadContext.handle = 0;

                        // Only send failure in the case of abort
                        if (!downloadContext.aborted)
                        {
                            switch (downloadContext.type)
                            {
                                case 0x325DC801: // "Updates"
                                    // Before giving up, try the other home
                                    if (RetryOnFallback())
                                    {
                                        break;
                                    }

                                    // Update failed
                                LOG_DIAG(("Update failed"));
                                    SendEvent
                                    (
                                        PrivData::updateCtrl, nullptr, IFace::NOTIFY,
                                        0x7CA15267
                                    ); // "Update::CheckFailed"
                                    break;

                                case 0x1770E157: // "Patch"
                                    // Patch failed
                                LOG_DIAG(("Patch failed"));
                                    SendEvent
                                    (
                                        PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                        0xB2623264
                                    ); // "Download::PatchFailed"
                                    break;

                                default:
                                    // Download failed
                                LOG_DIAG(("Download failed"));
                                    SendEvent
                                    (
                                        PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                        0x161F1710
                                    ); // "Download::Failed"
                                    break;
                            }
                        }
                    }
                    else if (failed->handle == motdContext.handle)
                    {
                        LOG_DIAG(("Failed to download message of the day"));

                        // Clear the context so that it will try again
                        motdContext.handle = NULL;
                    }

                    delete failed;
                    break;
                }

                default:
                ERR_FATAL(("Unknown message %08X", message));
            }
        }


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Struct Extra
        //


        //
        // Constructor
        //
        Extra::Extra(FScope* fScope)
        {
            name = StdLoad::TypeString(fScope);
            author = StdLoad::TypeString(fScope, "Author");
            size = StdLoad::TypeU32(fScope, "Size");
            file = StdLoad::TypeString(fScope, "File");

            FScope* sScope = fScope->GetFunction("Source", FALSE);

            if (sScope)
            {
                sourceHost = StdLoad::TypeString(sScope);
                sourcePort = StdLoad::TypeU32(sScope);
                sourcePath = StdLoad::TypeString(sScope);
            }
            else
            {
                sourceHost = updateHost.str;
                sourcePort = updatePort;
                sourcePath = updatePath.str;
            }
        }


        //
        // Download
        //
        void Download(Context& context)
        {
            ASSERT(initialized);

            // Is the host an IP
            if (Win32::Socket::Address::IsAddress(context.host.str))
            {
                // Make sure that the folder is there to receive the file
                FileDrive drive;
                FileDir dir;
                FileName name;
                FileExt ext;
                Dir::PathExpand(context.file.str, drive, dir, name, ext);
                if (!Dir::MakeFull(dir.str))
                {
                    ERR_FATAL(("Could not create download directory '%s'", dir.str));
                }

                // Prepend server path to file
                FilePath path;
                Utils::Strcpy(path.str, context.path.str);
                Utils::Strcat(path.str, context.file.str);

                // The name, never the resolved address: it is what the server's
                // certificate is checked against. The proxy is passed through
                // as configured - it used to be handed the target's own address
                // when no proxy was set, which the transport now reads as an
                // instruction to proxy through the target
                context.handle = WonIface::HTTPGet
                (
                    Settings::GetProxy(), context.name.str, context.port, path.str,
                    context.file.str, FALSE
                );
            }
            else
            {
                // Attempt a name lookup
                Win32::DNS::Host* host;
                GetByName(context.host.str, host, DNSCallback, &context);
            }
        }


        //
        // VerifyPatch
        //
        // Confirm a downloaded patch is the file the manifest described, before
        // anything arranges to execute it.
        //
        // Fails closed. A manifest entry with no hash is refused rather than
        // trusted, so that dropping the field - or an older manifest being
        // served from somewhere - cannot quietly turn verification off. On any
        // failure the file is deleted, so a later run cannot find it and assume
        // it was checked.
        //
        Bool VerifyPatch(const Patch& patch)
        {
            if (!*patch.hash.str)
            {
                LOG_ERR(("Refusing '%s': the manifest gave no hash for it", patch.file.str));
                File::Unlink(patch.file.str);
                return (FALSE);
            }

            U8 digest[SHA256::DIGEST_SIZE];

            if (!SHA256::FromFile(patch.file.str, digest))
            {
                LOG_ERR(("Refusing '%s': could not hash it", patch.file.str));
                File::Unlink(patch.file.str);
                return (FALSE);
            }

            if (!SHA256::Compare(digest, patch.hash.str))
            {
                char actual[SHA256::STRING_SIZE];
                SHA256::ToString(digest, actual, SHA256::STRING_SIZE);

                LOG_ERR(("Refusing '%s': expected %s, got %s", patch.file.str, patch.hash.str, actual));
                File::Unlink(patch.file.str);
                return (FALSE);
            }

            LOG_DIAG(("Verified '%s' against the manifest hash", patch.file.str));
            return (TRUE);
        }


        //
        // RetryOnFallback
        //
        // Repeat the manifest fetch against the fallback source. TRUE if a retry
        // was started, in which case the caller must not report failure yet.
        //
        Bool RetryOnFallback()
        {
            if (!haveFallback || usingFallback)
            {
                return (FALSE);
            }

            usingFallback = TRUE;

            LOG_DIAG(("Update check failed on '%s', trying '%s'", defaultHost.str, fallbackHost.str));

            // Anything the failed attempt managed to parse is discarded - the
            // fallback's manifest is the one we are going to act on
            patches.DisposeAll();
            extras.DisposeAll();
            patch = nullptr;

            updateHost = fallbackHost.str;
            updatePort = fallbackPort;
            updatePath = fallbackPath.str;

            Get
            (
                0x325DC801, // "Updates"
                fallbackHost.str,
                fallbackPort,
                fallbackPath.str,
                fileUpdates.str
            );

            return (TRUE);
        }


        //
        // Get patch
        //
        void GetPatch(const Patch& patch)
        {
            Get
            (
                0x1770E157, // "Patch"
                updateHost.str,
                updatePort,
                updatePath.str,
                patch.file.str
            );
        }


        //
        // Download a particular file
        //
        void Get(U32 type, const char* host, U16 port, const char* path, const char* file)
        {
            ASSERT(!downloadContext.handle);

            downloadContext.Reset();
            downloadContext.type = type;
            downloadContext.name = host;
            downloadContext.host = host;
            downloadContext.port = port;
            downloadContext.path = path;
            downloadContext.file = file;

            Download(downloadContext);
        }


        //
        // DNSCallback
        //
        void DNSCallback(const Win32::DNS::Host* host, void* c)
        {
            // Context is an unresolved directory server
            Context* context = static_cast<Context*>(c);

            if (!context->aborted)
            {
                if (host && host->GetAddress())
                {
                    LOG_DIAG(("Resolved host address '%s' to '%s'", context->host.str, host->GetAddress()->GetText()));

                    // The context is a download context, put the resolved 
                    // name into the context and start downloading
                    context->host = host->GetAddress()->GetText();

                    // Try to download now
                    Download(*context);
                }
                else
                {
                    LOG_DIAG(("Unresolved host address '%s'", context->host.str));

                    // A name that no longer resolves is the case the fallback
                    // exists for, so try it before anyone is told this failed.
                    // Checked outside the updateCtrl test below: whether a
                    // control happens to be registered has no bearing on which
                    // server we should be talking to
                    if (context == &downloadContext && downloadContext.type == 0x325DC801 && RetryOnFallback())
                    {
                        return;
                    }

                    // DNS failed
                    if (PrivData::updateCtrl.Alive())
                    {
                        // If it was the download context, let someone know
                        if (context == &downloadContext)
                        {
                            switch (downloadContext.type)
                            {
                                case 0x325DC801: // "Updates"
                                    // Update failed
                                    SendEvent
                                    (
                                        PrivData::updateCtrl, nullptr, IFace::NOTIFY,
                                        0x7CA15267
                                    ); // "Update::CheckFailed"
                                    break;

                                case 0x1770E157: // "Patch"
                                    // Patch failed
                                    SendEvent
                                    (
                                        PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                        0xB2623264
                                    ); // "Download::PatchFailed"
                                    break;

                                default:
                                    // Download failed
                                    SendEvent
                                    (
                                        PrivData::downloadCtrl, nullptr, IFace::NOTIFY,
                                        0x161F1710
                                    ); // "Download::Failed"
                                    break;
                            }
                        }
                        else
                        {
                            LOG_DIAG(("Unknown context"));
                        }
                    }
                }
            }
        }
    }
}
