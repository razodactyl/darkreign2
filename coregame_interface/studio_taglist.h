///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Tag listbox
//
// 24-NOV-1998
//


#ifndef __STUDIO_TAGLIST_H
#define __STUDIO_TAGLIST_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "iclistbox.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Studio
//
namespace Studio
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class TagListBox
    //
    class TagListBox : public ICListBox
    {
    PROMOTE_LINK(TagListBox, ICListBox, 0xB9F62A1C) // "TagListBox"

    public:

        // Constructor
        TagListBox(IControl* parent);
        ~TagListBox();

        // Event handling function
        U32 HandleEvent(Event& e);
    };
}

#endif
