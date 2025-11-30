///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Generic Grid Display Control
//
// 14-SEP-1998
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "icgrid.h"
#include "iface.h"
#include "iface_util.h"
#include "input.h"
#include "bitmap.h"


///////////////////////////////////////////////////////////////////////////////
//
// Class ICGrid - Control that displays custom colored grid
//


//
// Constructor
//
ICGrid::ICGrid(const char* name, U32 sx, U32 sy, U32 cx, U32 cy, IControl* parent) :
    IControl(parent),
    postPaintInfo(NULL),
    gridSize(sx, sy),
    cellSize(cx, cy),
    cellFunc(NULL),
    postFunc(NULL),
    eventFunc(NULL),
    context(NULL),
    xFlip(FALSE),
    yFlip(FALSE),
    displaySelected(FALSE),
    selected(0, 0)
{
    // Setup control 
    SetName(name);
    // Use SetGeomSize with design-space values - AdjustGeometry will scale
    SetGeomSize(gridSize.x * cellSize.x, gridSize.y * cellSize.y);
}


//
// DrawSelf
//
// Draw the control using currently registered callbacks
//
void ICGrid::DrawSelf(PaintInfo& pi)
{
    // Draw a frame
    DrawCtrlFrame(pi);

    // Do we have a cell iteration callback
    if (cellFunc)
    {
        // Scale cell size for rendering
        F32 scale = IFace::GetScale();
        U32 scaledCellX = U32(F32(cellSize.x) * scale);
        U32 scaledCellY = U32(F32(cellSize.y) * scale);

        // Window top left pixel position
        U32 xp1 = pi.client.p0.x;
        U32 yp1 = pi.client.p0.y;

        // Initial pixel positions for each axis (changes for flipping)
        U32 xip = xFlip ? xp1 + gridSize.x * scaledCellX - scaledCellX : xp1;
        U32 yip = yFlip ? yp1 + gridSize.y * scaledCellY - scaledCellY : yp1;

        // Draw the grid
        for (U32 x = 0, px = xip; x < gridSize.x; x++, px = xFlip ? (px - scaledCellX) : (px + scaledCellX))
        {
            for (U32 y = 0, py = yip; y < gridSize.y; y++, py = yFlip ? (py - scaledCellY) : (py + scaledCellY))
            {
                // Get the color from the callback
                Color color = cellFunc(context, x, y);

                // And paint this cell
                IFace::RenderRectangle(ClipRect(px, py, px + scaledCellX, py + scaledCellY), color);
            }
        }

        // Do we have a post-iteration callback
        if (postFunc)
        {
            // Setup paint info
            postPaintInfo = &pi;

            // Trigger the callback
            postFunc(this);

            // Clear data
            postPaintInfo = NULL;
        }

        // Hackville
        if (displaySelected)
        {
            U32 x = selected.x;
            U32 y = selected.y;
            if (xFlip) { x = gridSize.x - x - 1; }
            if (yFlip) { y = gridSize.y - y - 1; }
            U32 xPos = pi.client.p0.x + (x * scaledCellX) + (scaledCellX / 4);
            U32 yPos = pi.client.p0.y + (y * scaledCellY) + (scaledCellY / 4);
            IFace::RenderRectangle(ClipRect(xPos, yPos, xPos + scaledCellX / 2, yPos + scaledCellY / 2), Color(1.0F, 1.0F, 1.0F, 0.5F));
        }
    }
}


//
// DrawCell
//
// Only to be called from a post-iteration callback
//
void ICGrid::DrawCell(U32 x, U32 y, Color c)
{
    // Ensure we're in the right mode and have valid values
    if (postPaintInfo && (x < gridSize.x) && (y < gridSize.y))
    {
        // Scale cell size for rendering
        F32 scale = IFace::GetScale();
        U32 scaledCellX = U32(F32(cellSize.x) * scale);
        U32 scaledCellY = U32(F32(cellSize.y) * scale);

        // Adjust for flipped axis
        if (xFlip) { x = gridSize.x - x - 1; }
        if (yFlip) { y = gridSize.y - y - 1; }

        U32 xPos = postPaintInfo->client.p0.x + (x * scaledCellX);
        U32 yPos = postPaintInfo->client.p0.y + (y * scaledCellY);

        IFace::RenderRectangle(ClipRect(xPos, yPos, xPos + scaledCellX, yPos + scaledCellY), c);
    }
}


//
// CallEventHandler
//
// Call the custom event handler
//
void ICGrid::CallEventHandler(U32 event)
{
    if (eventFunc)
    {
        eventFunc(context, selected.x, selected.y, event);
    }
}


//
// HandleEvent
//
// Passes events through to the user
//
U32 ICGrid::HandleEvent(Event& e)
{
    if (e.type == Input::EventID())
    {
        // Input events
        switch (e.subType)
        {
            case Input::MOUSEBUTTONDOWN:
            case Input::MOUSEBUTTONDBLCLK:
            {
                // Is the click inside the client area
                if (InClient(Point<S32>(e.input.mouseX, e.input.mouseY)))
                {
                    // Ignore if we already have capture
                    if (IFace::GetCapture() != this)
                    {
                        // Grab capture
                        GetMouseCapture();

                        // Save mouse code
                        captureCode = e.input.code;
                    }
                }
                break;
            }

            case Input::MOUSEBUTTONUP:
            case Input::MOUSEBUTTONDBLCLKUP:
            {
                if (HasMouseCapture() && e.input.code == captureCode)
                {
                    ReleaseMouseCapture();

                    // Get mouse position relative to client window
                    Point<S32> p = ScreenToClient(Point<S32>(e.input.mouseX, e.input.mouseY));

                    // Scale cell size for hit detection
                    F32 scale = IFace::GetScale();
                    U32 scaledCellX = U32(F32(cellSize.x) * scale);
                    U32 scaledCellY = U32(F32(cellSize.y) * scale);

                    // Calculate the (flipped) cell positions using scaled cell sizes
                    U32 x = xFlip ? gridSize.x - (p.x / scaledCellX) - 1 : p.x / scaledCellX;
                    U32 y = yFlip ? gridSize.y - (p.y / scaledCellY) - 1 : p.y / scaledCellY;

                    // Set currently selected
                    if (x < gridSize.x && y < gridSize.y)
                    {
                        selected.Set(x, y);
                    }

                    if (captureCode == Input::LeftButtonCode())
                    {
                        CallEventHandler(0x90E4DA5D); // "LeftClick"
                    }
                    else if (captureCode == Input::MidButtonCode())
                    {
                        CallEventHandler(0x316EC946); // "MiddleClick"
                    }
                    else if (captureCode == Input::RightButtonCode())
                    {
                        CallEventHandler(0x173F5F78); // "RightClick"
                    }

                    return (TRUE);
                }
                break;
            }
        }
    }

    // Allow parent class to process this event
    return IControl::HandleEvent(e);
}
