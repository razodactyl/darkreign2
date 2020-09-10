///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// File Mapping
// 23-MAR-1999
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "filemap.h"


///////////////////////////////////////////////////////////////////////////////
//
// Class FileMap
//


//
// FileMap::FileMap
//
// Constructor
//
FileMap::FileMap(const char* name, U32 flags, U32 offset, U32 length)
{
    // Build flags from flags
    U32 f = 0;
    if (flags & READ)
    {
        f |= GENERIC_READ;
    }
    if (flags & WRITE)
    {
        f |= GENERIC_WRITE;
    }

    // Open the file
    fileHandle = CreateFile(name, f, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (fileHandle == nullptr)
    {
        ERR_FATAL(("Could not open file '%s' for mapping", name))
    }

    // Get the size of the file (assume that its is less than 4GB)
    size = GetFileSize(fileHandle, nullptr);

    // Create the file mapping
    f = 0;
    if (flags & WRITE)
    {
        f = PAGE_READWRITE;
    }
    else
    {
        f = PAGE_READONLY;
    }
    mapHandle = CreateFileMapping(fileHandle, nullptr, f, 0, size, nullptr);

    // Could the file be mapped
    if (mapHandle == nullptr)
    {
        ERR_FATAL(("Could not create file mapping for '%s'", name))
    }

    // Get a pointer to the mapping
    f = 0;
    if (flags & WRITE)
    {
        f = FILE_MAP_WRITE;
    }
    else
    {
        f = FILE_MAP_READ;
    }
    ptr = static_cast<U8*>(MapViewOfFile(mapHandle, f, 0, offset, length));

    // Could we get a view of the mapping ?
    if (ptr == nullptr)
    {
        ERR_FATAL(("Could not create a view of the mapping '%s'", name))
    }
}


//
// FileMap::~FileMap
//
// Destructor
//
FileMap::~FileMap()
{
    // Unmap pointer to mapping
    UnmapViewOfFile(ptr);

    // Close Mapping
    CloseHandle(mapHandle);

    // Close File
    CloseHandle(fileHandle);
}


//
// FileMap::GetSize
//
// Get pointer to mapping
//
U32 FileMap::GetSize()
{
    return (size);
}


//
// FileMap::GetPtr
//
// Get pointer to mapping
//
U8* FileMap::GetPtr()
{
    return (ptr);
}
