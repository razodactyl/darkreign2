// Minimal stand-in for the engine's utiltypes.h + std.h forced include,
// enough to compile interface/pixelscale.cpp on its own.
#ifndef __STUB_UTILTYPES_H
#define __STUB_UTILTYPES_H

#include <cstring>
#include <cstdlib>

typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef int S32;
typedef short S16;
typedef char S8;
typedef float F32;
typedef int Bool;

#define TRUE 1
#define FALSE 0

#define ASSERT(x)

namespace Utils
{
    inline int Stricmp(const char* a, const char* b) { return _stricmp(a, b); }
    inline void* Memcpy(void* d, const void* s, size_t n) { return memcpy(d, s, n); }
}

#endif
