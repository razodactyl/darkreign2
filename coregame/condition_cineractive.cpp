///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Condition System
//
// 11-AUG-1998
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "condition.h"
#include "condition_private.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Conditions
//
namespace Conditions
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class SoundEffect
    //

    //
    // SoundEffect::SoundEffect
    //
    SoundEffect::SoundEffect(FScope* fScope)
        : Condition(fScope)
    {
        name = StdLoad::TypeString(fScope, "Name");
    }


    //
    // SoundEffect::Test
    //
    Bool SoundEffect::Test(Team*)
    {
        return (TRUE);
    }
}
