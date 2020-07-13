#include "pch.h"

#include "Encoding.h"


namespace MINTCLIENT
{
    namespace Encoding
    {
        int TLV::ReadType(char type)
        {
            this->type = static_cast<unsigned int>(type);
            return this->type;
        }

        //
        // Convenience function to cast U8 to bool.
        //
        bool TLV::GetU8Bool()
        {
            const U8 val = this->GetU8();
            return static_cast<bool>(val);
        }

        U8 TLV::GetU8()
        {
            ASSERT(this->length >= 1);
            ASSERT(this->type == TLV::TLV_UINT8);

            // Data offset for the current item.
            unsigned int cur = this->offset - this->length;

            return this->data[cur];
        }

        //
        // [04] [00 00 00 04] [11 22 33 44]
        //
        U32 TLV::GetU32()
        {
            ASSERT(this->length >= 4);
            ASSERT(this->type == TLV::TLV_UINT32);

            // Data offset for the current item.
            unsigned int cur = this->offset - this->length;

            U32 c1 = this->data[cur + 0] << 24; // 11 << 24     = 0x11000000
            U32 c2 = this->data[cur + 1] << 16; // 22 << 16     = 0x00220000
            U32 c3 = this->data[cur + 2] << 8;  // 33 << 8      = 0x00003300
            U32 c4 = this->data[cur + 3] << 0;  // 44 << 0      = 0x00000044

            U32 ret = c1 | c2 | c3 | c4;

            return ret;
        }

        CH* TLV::GetString()
        {
            ASSERT(this->type == TLV::TLV_STRING);

            auto* chars = new unsigned short[this->length + 1];
            Utils::Memset(chars, 0, this->length + 1);

            Utils::Strmcpy(chars, (CH*)(this->data + 5), this->length + 1);

            return chars;
        }

        U8* TLV::GetBytes()
        {
            ASSERT(this->type == TLV::TLV_BYTES);

            auto* chars = new U8[this->length];
            Utils::Memset(chars, 0, this->length);
            Utils::Memcpy(chars, this->data + 5, this->length);

            return chars;
        }
    }
}
