// Stand-in for graphics/vid_public.h - ScaleBitmapUI only reads the device's
// maximum texture dimensions from it.
#ifndef __STUB_VID_PUBLIC_H
#define __STUB_VID_PUBLIC_H

#include "utiltypes.h"

namespace Vid
{
    struct Caps
    {
        U32 maxTexWid;
        U32 maxTexHgt;
    };

    extern Caps caps;
}

#endif
