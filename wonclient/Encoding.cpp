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

        U32 TLV::GetU32()
        {
            ASSERT(this->length >= 4);
            ASSERT(this->type == TLV::TLV_UINT32);

            unsigned int offset = this->DataOffset();

            return ((this->data[offset + 0] & 0xFFFFFFFF) + (this->data[offset + 1] & 0xFFFFFFFF) + (this->data[offset + 2] & 0xFFFFFFFF) + (this->data[offset + 3] & 0xFFFFFFFF));
        }

        CH* TLV::GetString()
        {
            ASSERT(this->type == TLV::TLV_STRING);

            auto* chars = new unsigned short[this->length + 1];
            Utils::Memset(chars, 0, this->length + 1);

            Utils::Strmcpy(chars, (CH*)(this->data + 5), this->length + 1);

            return chars;
        }
    }
}

