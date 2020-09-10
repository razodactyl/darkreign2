///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Order Lag
//


#ifndef __CLIENT_ORDERLAG_H
#define __CLIENT_ORDERLAG_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "icwindow.h"
#include "ifvar.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Client
//
namespace Client
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class OrderLag
    //
    class OrderLag : public ICWindow
    {
    public:

        // Constructor
        OrderLag(IControl* parent);

        // Destructor
        ~OrderLag();

        // Redraw self
        void DrawSelf(PaintInfo& pi);
    };
}


#endif
