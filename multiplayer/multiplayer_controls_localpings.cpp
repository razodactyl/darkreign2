///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// MultiPlayer Stuff
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "multiplayer_controls_localpings.h"
#include "multiplayer_pingdisplay.h"
#include "multiplayer_private.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace MultiPlayer
//
namespace MultiPlayer
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Controls
    //
    namespace Controls
    {
        ///////////////////////////////////////////////////////////////////////////////
        //
        // Class LocalPings
        //

        //
        // Redraw self
        //
        void LocalPings::DrawSelf(PaintInfo& pi)
        {
            PingDisplay::Draw(PrivData::maxLocalPings, PrivData::localPings, pi.client, pi.alphaScale);
        }
    }
}
