///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Team List Editor
//
// 24-NOV-1998
//


#ifndef __STUDIO_ATTACHLIST_H
#define __STUDIO_ATTACHLIST_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "iclistbox.h"


///////////////////////////////////////////////////////////////////////////////
//
// Forward Declarations
//
class Team;


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Studio
//
namespace Studio
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class AttachList
    //
    class AttachList : public ICListBox
    {
    PROMOTE_LINK(AttachList, ICListBox, 0x68371F13); // "AttachList"

    public:

        // Constructor
        AttachList(IControl* parent);
        ~AttachList();

        // Build the list of attachments using the given type
        void BuildList(MapObj* obj);
    };
}

#endif
