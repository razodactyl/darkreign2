///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1998
// Matthew Versluys
//
// Logging System
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "logging_destconsole.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Logging
//
namespace Logging
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class DestConsole
    //


    //
    // DestConsole::DestConsole
    //
    DestConsole::DestConsole()
    {
    }


    //
    // DestConsole::~DestConsole
    //
    DestConsole::~DestConsole()
    {
    }


    //
    // DestConsole::Write
    //
    // Write to the destination
    //
    void DestConsole::Write(Level, const char*, const char*, U32, U32, const char* message)
    {
        std::cout << endl << message << ends;
    }
}
