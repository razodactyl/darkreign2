///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// File System Resource Stream Management
//
// 16-DEC-1997
//

#ifndef __FILESRCDIR_H
#define __FILESRCDIR_H


//
// Includes
//
#include "filesys_private.h"


namespace FileSys
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class FileSrcDir - Direct access file source
    //
    class FileSrcDir : public FileSrc
    {
    protected:

        // Directory to load files from
        PathIdent dirId;

        // Index of known files
        BinTree<void> index;

    protected:

        // For accessing files from this source type
        class DataFileDir : public DataFile
        {
        protected:

            // File object
            File file;

            // Memory pointer
            void* memoryPtr;

            // File source containing this file
            Reaper<FileSrcDir> source;

        public:

            // Constructor and destructor
            DataFileDir(FileSrcDir* source, const char* name);
            ~DataFileDir();

            // Was the file successfully opened
            Bool IsOpen();

            // Required overriding methods
            U32 Size() override;
            U32 Read(void* dst, U32 size) override;
            Bool Seek(U32 offset) override;
            U32 FilePos() override;
            void* GetMemoryPtr() override;
            const char* Path() override;
        };

        // For quick finds on files from this source type
        class FastFindDir : public FastFind
        {
        protected:

            // Required method
            DataFile* Open() override;

            // File source that this file is within
            FileSrcDir* source;

        public:

            FastFindDir(const char* name, U32 size, ResourceStream* stream, FileSrcDir* source) :
                FastFind(name, size, stream),
                source(source)
            {
            }
        };

    public:

        // Constructor and destructor
        FileSrcDir(const char* dir);
        ~FileSrcDir();

        // Required overriding methods
        void BuildIndex() override;
        Bool Exists(U32 crc) override;
        FastFind* GetFastFind(const char* name, ResourceStream* stream) override;
        DataFile* Open(const char* name) override;
        const char* Path() override;
        void LogSource(U32 indent) override;
    };
}

#endif
