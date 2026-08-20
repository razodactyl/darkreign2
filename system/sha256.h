///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Secure Hash
//
// Used to verify downloaded updates before they are executed - see
// docs/update-system.md. Backed by the Windows CNG provider rather than a
// hash of our own, so there is one less piece of crypto to get wrong.
//

#ifndef __SHA256_H
#define __SHA256_H


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace SHA256
//
namespace SHA256
{
    // Size of a raw digest in bytes
    const U32 DIGEST_SIZE = 32;

    // Size of a buffer able to hold a digest as hex text, including the null
    const U32 STRING_SIZE = DIGEST_SIZE * 2 + 1;

    // Hash the contents of a file. FALSE if it could not be read or hashed,
    // in which case the digest is left untouched.
    Bool FromFile(const char* path, U8 digest[DIGEST_SIZE]);

    // Hash a buffer. FALSE if the provider could not be used.
    Bool FromBuffer(const void* data, U32 size, U8 digest[DIGEST_SIZE]);

    // Convert a digest to lowercase hex text. 'dest' must hold STRING_SIZE.
    void ToString(const U8 digest[DIGEST_SIZE], char* dest, U32 size);

    // Compare a digest against hex text, ignoring case. FALSE if the text is
    // not exactly DIGEST_SIZE * 2 hex characters, so malformed manifest
    // entries never compare equal.
    Bool Compare(const U8 digest[DIGEST_SIZE], const char* text);
}

#endif
