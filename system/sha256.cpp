///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Secure Hash
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "sha256.h"
#include "file.h"
#include <bcrypt.h>


///////////////////////////////////////////////////////////////////////////////
//
// Libraries
//
#pragma comment(lib, "bcrypt.lib")


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace SHA256
//
namespace SHA256
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Definitions
    //

    // Amount of the file read per pass
    #define READ_BLOCK 32768


    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class Hasher
    //
    // Owns the CNG algorithm and hash handles so that no return path can leak
    // them - there are a lot of ways for the calls below to fail.
    //
    class Hasher
    {
    private:

        BCRYPT_ALG_HANDLE alg;
        BCRYPT_HASH_HANDLE hash;
        U8* object;

    public:

        Hasher()
            : alg(nullptr),
              hash(nullptr),
              object(nullptr)
        {
        }

        ~Hasher()
        {
            if (hash)
            {
                BCryptDestroyHash(hash);
            }
            if (object)
            {
                delete[] object;
            }
            if (alg)
            {
                BCryptCloseAlgorithmProvider(alg, 0);
            }
        }

        //
        // Open the provider and create a hash. FALSE if unavailable.
        //
        Bool Open()
        {
            if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            {
                LOG_ERR(("Could not open the SHA256 provider"));
                alg = nullptr;
                return (FALSE);
            }

            // Ask how much scratch space the provider wants
            DWORD objectSize = 0;
            ULONG copied = 0;

            if (!BCRYPT_SUCCESS(
                BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectSize, sizeof(objectSize), &copied, 0)))
            {
                LOG_ERR(("Could not size the SHA256 hash object"));
                return (FALSE);
            }

            object = new U8[objectSize];

            if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, object, objectSize, nullptr, 0, 0)))
            {
                LOG_ERR(("Could not create a SHA256 hash"));
                hash = nullptr;
                return (FALSE);
            }

            return (TRUE);
        }

        //
        // Add data to the hash
        //
        Bool Append(const void* data, U32 size)
        {
            return (BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)data, size, 0)) ? TRUE : FALSE);
        }

        //
        // Take the digest. The hash cannot be reused afterwards.
        //
        Bool Finish(U8 digest[DIGEST_SIZE])
        {
            return (BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, DIGEST_SIZE, 0)) ? TRUE : FALSE);
        }
    };


    //
    // Hash the contents of a file
    //
    Bool FromFile(const char* path, U8 digest[DIGEST_SIZE])
    {
        ASSERT(path);

        File file;

        if (!file.Open(path, File::READ))
        {
            LOG_ERR(("Could not open '%s' to hash it", path));
            return (FALSE);
        }

        Hasher hasher;

        if (!hasher.Open())
        {
            file.Close();
            return (FALSE);
        }

        U8* buffer = new U8[READ_BLOCK];
        Bool ok = TRUE;

        for (;;)
        {
            U32 read = file.Read(buffer, READ_BLOCK);

            if (!read)
            {
                break;
            }

            if (!hasher.Append(buffer, read))
            {
                LOG_ERR(("Failed while hashing '%s'", path));
                ok = FALSE;
                break;
            }
        }

        delete[] buffer;
        file.Close();

        return (ok ? hasher.Finish(digest) : FALSE);
    }


    //
    // Hash a buffer
    //
    Bool FromBuffer(const void* data, U32 size, U8 digest[DIGEST_SIZE])
    {
        Hasher hasher;

        if (!hasher.Open() || !hasher.Append(data, size))
        {
            return (FALSE);
        }

        return (hasher.Finish(digest));
    }


    //
    // Convert a digest to lowercase hex text
    //
    void ToString(const U8 digest[DIGEST_SIZE], char* dest, U32 size)
    {
        ASSERT(dest);
        ASSERT(size >= STRING_SIZE);

        // Checked in release too - the caller supplying a short buffer must not
        // turn into a silent overrun of it
        if (size < STRING_SIZE)
        {
            if (size)
            {
                dest[0] = '\0';
            }
            return;
        }

        static const char* hex = "0123456789abcdef";

        for (U32 i = 0; i < DIGEST_SIZE; i++)
        {
            dest[i * 2] = hex[digest[i] >> 4];
            dest[i * 2 + 1] = hex[digest[i] & 0x0F];
        }

        dest[DIGEST_SIZE * 2] = '\0';
    }


    //
    // Compare a digest against hex text
    //
    Bool Compare(const U8 digest[DIGEST_SIZE], const char* text)
    {
        if (!text || Utils::Strlen(text) != DIGEST_SIZE * 2)
        {
            return (FALSE);
        }

        char str[STRING_SIZE];
        ToString(digest, str, STRING_SIZE);

        // Both sides are plain hex, so a case insensitive compare is enough
        return (Utils::Stricmp(str, text) ? FALSE : TRUE);
    }
}
