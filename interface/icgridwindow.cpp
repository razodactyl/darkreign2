///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Window wrapper for an ICGrid
//
// 24-NOV-1998
//


//
// Includes
//
#include "icgridwindow.h"
#include "iface.h"
#include "iface_types.h"


///////////////////////////////////////////////////////////////////////////////
//
// Class ICGridWindow - Wrapper for an ICGrid
//


//
// Constructor
//
ICGridWindow::ICGridWindow(const char* name, U32 sx, U32 sy, U32 cx, U32 cy)
    : ICWindow(IFace::RootWindow())
{
    // Create the grid
    grid = new ICGrid("Grid", sx, sy, cx, cy, this);

    // Setup the window - use design-space values
    SetName(name);
    // Grid size is in design-space (gridSize * cellSize), title height is also design-space
    SetGeomSize(sx * cx, sy * cy + IFace::GetMetric(IFace::THIN_TITLE_HEIGHT));
    SetStyle("ThinTitleBar", NULL);
}


//
// Deactivate
//
Bool ICGridWindow::Deactivate()
{
    if (ICWindow::Deactivate())
    {
        // Mark for deletion
        MarkForDeletion();

        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}
