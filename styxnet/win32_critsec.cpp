///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1999
// Matthew Versluys
//
// Win32 CritSec
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "win32_critsec.h"


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


    //
    // Constructor
    //
    CritSec::CritSec()
    {
        InitializeCriticalSection(&critSect);
    }


    //
    // Destructor
    //
    CritSec::~CritSec()
    {
        DeleteCriticalSection(&critSect);
    }


    //
    // Enter
    //
    void CritSec::Enter()
    {
        EnterCriticalSection(&critSect);
    }


    //
    // Exit
    //
    void CritSec::Exit()
    {
        LeaveCriticalSection(&critSect);
    }
}
