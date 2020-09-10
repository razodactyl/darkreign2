///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// MultiPlayer Stuff
// 1-JUL-1999
//


#ifndef __MULTIPLAYER_CONTROLS_REPORT_H
#define __MULTIPLAYER_CONTROLS_REPORT_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "icwindow.h"
#include "ifvar.h"


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
        // Class Report
        //
        class Report : public ICWindow
        {
        public:

            // Constructor
            Report(IControl* parent);

            // Destructor
            ~Report();

            // Redraw self
            void DrawSelf(PaintInfo& pi);
        };
    }
}


#endif
