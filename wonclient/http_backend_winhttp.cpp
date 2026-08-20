///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// http_backend_winhttp.cpp     download transport, Windows WinHTTP
//
// The default backend. TLS, certificate chain validation and hostname checking
// are all done by the operating system, so there is no CA bundle shipped with
// the game and nothing to keep current as roots rotate.
//
// This file also holds SelectBackend, the same way vid_backend_d3d.cpp holds
// the graphics one: the default backend owns the switch so that a table is
// always installed even if selection never runs.
//

#include "pch.h"

#include "http_backend.h"
#include "file.h"

#include <windows.h>
#include <winhttp.h>


///////////////////////////////////////////////////////////////////////////////
//
// Libraries
//
#pragma comment(lib, "winhttp.lib")


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace MINTCLIENT
//
namespace MINTCLIENT
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace HTTP
    //
    namespace HTTP
    {
        ///////////////////////////////////////////////////////////////////////////////
        //
        // Definitions
        //

        // Amount read from the connection per pass
        #define READ_BLOCK 16384

        // Milliseconds before an idle connection is given up on
        #define TIMEOUT_MS 30000


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Internal Data
        //

        static const char* userAgent = "DarkReign2";


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Class Wide
        //
        // WinHTTP is a wide character API and everything handed to us is ANSI.
        //
        class Wide
        {
        private:

            wchar_t* str;

        public:

            Wide(const char* text)
                : str(nullptr)
            {
                if (!text)
                {
                    return;
                }

                int chars = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);

                if (chars > 0)
                {
                    str = new wchar_t[chars];
                    MultiByteToWideChar(CP_ACP, 0, text, -1, str, chars);
                }
            }

            ~Wide()
            {
                delete[] str;
            }

            operator const wchar_t*() const
            {
                return (str);
            }

            const wchar_t* Str() const
            {
                return (str);
            }
        };


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Class Handle
        //
        // WinHTTP handles have to be closed on every exit path, of which there
        // are a lot below.
        //
        class Handle
        {
        private:

            HINTERNET handle;

        public:

            Handle()
                : handle(nullptr)
            {
            }

            ~Handle()
            {
                if (handle)
                {
                    WinHttpCloseHandle(handle);
                }
            }

            Handle& operator=(HINTERNET h)
            {
                if (handle)
                {
                    WinHttpCloseHandle(handle);
                }
                handle = h;
                return (*this);
            }

            operator HINTERNET() const
            {
                return (handle);
            }

            Bool Alive() const
            {
                return (handle ? TRUE : FALSE);
            }
        };


        //
        // Fetch over TLS into a file
        //
        static Error WinHttpGet(const Request& request)
        {
            if (!request.host || !request.path || !request.saveAsPath)
            {
                return (Errors::GeneralFailure);
            }

            Handle session;
            Wide proxy(request.proxy);

            // An explicitly configured proxy wins; otherwise take whatever the
            // machine is already set up to use, which is what a player behind a
            // corporate or school connection will need
            if (request.proxy && *request.proxy)
            {
                session = WinHttpOpen
                (
                    Wide(userAgent), WINHTTP_ACCESS_TYPE_NAMED_PROXY,
                    proxy.Str(), WINHTTP_NO_PROXY_BYPASS, 0
                );
            }
            else
            {
                session = WinHttpOpen
                (
                    Wide(userAgent), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0
                );
            }

            if (!session.Alive())
            {
                LOG_ERR(("WinHttpOpen failed (%d)", GetLastError()));
                return (Errors::GeneralFailure);
            }

            WinHttpSetTimeouts(session, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS);

            Handle connect;
            connect = WinHttpConnect(session, Wide(request.host), request.port, 0);

            if (!connect.Alive())
            {
                LOG_ERR(("WinHttpConnect to '%s:%d' failed (%d)", request.host, request.port, GetLastError()));
                return (Errors::GeneralFailure);
            }

            // WINHTTP_FLAG_SECURE is the whole point of this backend. Note that
            // certificate validation is on by default and is deliberately never
            // relaxed here - no WINHTTP_OPTION_SECURITY_FLAGS, no ignoring of
            // unknown CAs or name mismatches
            Handle req;
            req = WinHttpOpenRequest
            (
                connect, L"GET", Wide(request.path), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE
            );

            if (!req.Alive())
            {
                LOG_ERR(("WinHttpOpenRequest for '%s' failed (%d)", request.path, GetLastError()));
                return (Errors::GeneralFailure);
            }

            // Follow redirects, but never one that leaves TLS. This is the
            // WinHTTP default; set it explicitly so it survives someone tidying
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
            WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            {
                LOG_ERR(("WinHttpSendRequest to '%s' failed (%d)", request.host, GetLastError()));
                return (Errors::GeneralFailure);
            }

            if (!WinHttpReceiveResponse(req, nullptr))
            {
                U32 error = GetLastError();

                // The one failure worth naming: the server presented something
                // the machine will not trust
                if (error == ERROR_WINHTTP_SECURE_FAILURE)
                {
                    LOG_ERR(("TLS validation failed for '%s' - refusing the download", request.host));
                }
                else
                {
                    LOG_ERR(("WinHttpReceiveResponse from '%s' failed (%d)", request.host, error));
                }
                return (Errors::GeneralFailure);
            }

            // Status
            DWORD status = 0;
            DWORD statusSize = sizeof(status);

            if (!WinHttpQueryHeaders
            (
                req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX
            ))
            {
                LOG_ERR(("Could not read the status from '%s' (%d)", request.host, GetLastError()));
                return (Errors::GeneralFailure);
            }

            if (status != 200)
            {
                LOG_ERR(("Got status %d for '%s%s'", status, request.host, request.path));
                return (Errors::GeneralFailure);
            }

            // Declared length, if there is one. Chunked responses report none,
            // which is legal - progress simply has no total to report
            DWORD size = 0;
            DWORD sizeSize = sizeof(size);

            if (!WinHttpQueryHeaders
            (
                req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &size, &sizeSize, WINHTTP_NO_HEADER_INDEX
            ))
            {
                size = 0;
            }

            File file;

            if (!file.Open(request.saveAsPath, File::CREATE | File::WRITE))
            {
                LOG_ERR(("Could not create '%s'", request.saveAsPath));
                return (Errors::GeneralFailure);
            }

            U8* buffer = new U8[READ_BLOCK];
            U32 transferred = 0;
            Bool ok = TRUE;

            for (;;)
            {
                DWORD read = 0;

                if (!WinHttpReadData(req, buffer, READ_BLOCK, &read))
                {
                    LOG_ERR(("WinHttpReadData from '%s' failed (%d)", request.host, GetLastError()));
                    ok = FALSE;
                    break;
                }

                if (!read)
                {
                    break;
                }

                if (file.Write(buffer, read) != read)
                {
                    LOG_ERR(("Could not write to '%s' - out of space?", request.saveAsPath));
                    ok = FALSE;
                    break;
                }

                transferred += read;

                if (request.OnProgress && !request.OnProgress(transferred, size, request.context))
                {
                    LOG_DIAG(("Download of '%s' aborted by the caller", request.path));
                    ok = FALSE;
                    break;
                }
            }

            delete[] buffer;
            file.Close();

            // A truncated body is a failure, not a short file. Catching it here
            // means nothing downstream ever sees a half written installer
            if (ok && size && transferred != size)
            {
                LOG_ERR(("'%s' ended early - %d of %d bytes", request.path, transferred, size));
                ok = FALSE;
            }

            if (!ok)
            {
                File::Unlink(request.saveAsPath);
                return (Errors::GeneralFailure);
            }

            return (Errors::Success);
        }


        ///////////////////////////////////////////////////////////////////////////////
        //
        // The table
        //
        static const Backend winHttpBackend =
        {
            "WinHTTP",
            backendWinHttp,
            WinHttpGet,
        };


        // Installed up front so that anything running before selection still has
        // a table to call, exactly as the graphics backend does
        const Backend* backend = &winHttpBackend;


        //
        // SelectBackend
        //
        void SelectBackend(BackendId id)
        {
            switch (id)
            {
                case backendWinHttp:
                    backend = &winHttpBackend;
                    break;

                default:
                    ERR_FATAL(("SelectBackend: unknown HTTP backend %d", id));
            }

            LOG_DIAG(("HTTP transport: %s", backend->name));
        }
    }
}
