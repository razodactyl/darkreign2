#pragma once

#include "MINTCLIENT.h"

#include "utils.h"
#include "defines.h"
#include "debug.h"


namespace MINTCLIENT
{
    namespace Encoding
    {
        struct TLV
        {
            static const unsigned int TLV_UNDEFINED = 0;
            static const unsigned int TLV_ARRAY = 1;
            static const unsigned int TLV_UINT8 = 2;
            static const unsigned int TLV_UINT16 = 3;
            static const unsigned int TLV_UINT32 = 4;
            static const unsigned int TLV_STRING = 5;

            int type;
            unsigned int length;
            unsigned int offset = 0;
            U8* data;
            TLV* items;

            TLV()
            {
                //
            }

            TLV(const U8* bytes, unsigned int size)
            {
                // TLV with 0 length makes no sense.
                ASSERT(size > 0);

                // Allocate len amount of bytes.
                this->data = new U8[size];
                Utils::Memcpy(this->data, bytes, size);

                // Pointer to first byte of data.
                U8* p = data;

                // It determines the type.
                const int type = ReadType(*p);

                // Second 4 bytes determine the length.
                this->length = (p[1] << 24 & 0xFF000000) + (p[2] << 16 & 0x00FF0000) + (p[3] & 0x0000FF00) + (p[4] & 0x000000FF);

                // The rest of the data follows.
                this->offset += 5;
                p = &p[this->offset];

                // If it's an array.
                if (type == TLV_ARRAY)
                {
                    // Allocate `length` entries.
                    items = new TLV[length];

                    for (unsigned int i = 0; i < length; i++)
                    {
                        auto remaining = size - this->offset;
                        items[i] = TLV(p, remaining);
                        p += items[i].DataOffset();
                        this->offset += items[i].DataOffset();
                    }
                }
                else
                {
                    // Length contains the size of the data.
                    if (type == TLV_STRING)
                    {
                        // What are we working with.
                        Utils::MemoryDump(p, length * 2);

                        this->offset += (length * 2);
                    }
                }
            }

            ~TLV()
            {
                if (this->length > 0)
                {
                    if (this->type == TLV_ARRAY)
                    {
                        //delete[] items;
                    }
                    else
                    {
                        //delete data;
                    }
                }
            }

            int ReadType(char type);
            unsigned int DataOffset()
            {
                return this->offset;
            }

            U32 GetU32();
            CH* GetString();
        };
    }
}
