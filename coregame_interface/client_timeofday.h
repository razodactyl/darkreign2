///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Time of day display
//
// 05-APR-2000
//


#ifndef __CLIENT_TIMEOFDAY_H
#define __CLIENT_TIMEOFDAY_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "icontrol.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Client
//
namespace Client
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class TimeOfDay - Time of day display
    //
    class TimeOfDay : public IControl
    {
    public:

        // Constructor
        TimeOfDay(IControl* parent);
        ~TimeOfDay();

        void DrawSelf(PaintInfo& pi);
    };
}

#endif
