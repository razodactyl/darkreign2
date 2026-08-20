///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// http_backend.h     download transport dispatch
//
// MINTCLIENT::Directory fetches the update manifest, the message of the day and
// the patch installers through the table of function pointers below rather than
// calling an HTTP library directly. There is one table per backend;
// `HTTP::backend` points at whichever was selected at startup.
//
// Backends:
//   http_backend_winhttp.cpp   Windows WinHTTP, the default
//
// WinHTTP is the default because TLS and certificate validation come from the
// operating system: no CA bundle to ship or keep current, and no third party
// crypto in a 32 bit /MT build. An OpenSSL backend over cpp-httplib can be
// added beside it without the callers changing, in the same way graphics gained
// an OpenGL backend beside DirectX - see graphics/vid_backend.h.
//
// Every backend must be TLS only. The update mechanism executes what it
// downloads (see docs/update-system.md), so a backend that can be talked into
// plaintext is a backend that can be talked into running someone else's
// installer. A backend is chosen once and does not change for the lifetime of
// the process.
//

#ifndef __HTTP_BACKEND_H
#define __HTTP_BACKEND_H

#include "MINTCLIENT.h"


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
        // Enum BackendId
        //
        enum BackendId
        {
            backendWinHttp,
        };


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Struct Request
        //
        struct Request
        {
            // Host to connect to. Must be the DNS name rather than a resolved
            // address: it is what the certificate is checked against, and what
            // is sent as SNI. Handing a backend an address here means handing it
            // a certificate that cannot match.
            const char* host;

            // Port on the host. 443 unless the server is somewhere unusual.
            unsigned short port;

            // Absolute path to request, including any query
            const char* path;

            // Proxy as "host:port", or null/empty to use the system settings
            const char* proxy;

            // Where the response body is written. Truncated on success, and
            // removed if the transfer does not complete, so a failed download
            // never leaves a partial file that a later step might trust.
            const char* saveAsPath;

            // Called as the body arrives, with the bytes so far and the total
            // when the server declared one (0 when it did not). Returning FALSE
            // aborts the transfer, which then reports Error_GeneralFailure.
            bool (*OnProgress)(unsigned long progress, unsigned long size, void* context);

            // Passed back to OnProgress untouched
            void* context;
        };


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Struct Backend
        //
        struct Backend
        {
            // identity
            //
            const char* name;
            BackendId id;

            // Fetch request.path over TLS into request.saveAsPath, blocking
            // until it finishes. Called on the download thread, never on the
            // main thread. Returns Error_Success, or the closest MINTCLIENT
            // error - anything other than Error_Success means the caller must
            // treat the destination file as absent.
            Error (*Get)(const Request& request);
        };


        // The table in use; never null after SelectBackend
        extern const Backend* backend;

        // Choose a backend. Called once, before the first Get.
        void SelectBackend(BackendId id);
    }
}

#endif
