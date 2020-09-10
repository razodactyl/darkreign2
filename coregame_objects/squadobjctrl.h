///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Game-Play Engine
//
// 10-JUL-1998
//

#ifndef __SQUADOBJCTRL_H
#define __SQUADOBJCTRL_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "squadobjdec.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace SquadObjCtrl
//
namespace SquadObjCtrl
{
    // Initialize system
    void Init();

    // Shutdown system
    void Done();

    // Create a squad
    SquadObj* Create(Team* team);
}

#endif
