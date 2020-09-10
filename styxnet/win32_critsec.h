///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1999
// Matthew Versluys
//
// Win32 Critical Section
//


#ifndef __WIN32_CRITSEC_H
#define __WIN32_CRITSEC_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "win32.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Win32
//
namespace Win32
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class CritSec
    //
    class CritSec
    {
    private:

        // The critical section
        CRITICAL_SECTION critSect;

    public:

        // Constructor
        CritSec();

        // Destructor
        ~CritSec();

        // Enter the critical section
        void Enter();

        // Exit the critical section
        void Exit();
    };
}

#endif
