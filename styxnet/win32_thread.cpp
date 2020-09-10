///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1998
// Matthew Versluys
//
// Win32 Thread
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "win32_thread.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Win32
//
namespace Win32
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Class Thread
    //


    //
    // Thread
    //
    // Default constructor
    //
    Thread::Thread()
        : threadID(0),
          threadHandle(nullptr)
    {
    }


    //
    // Thread
    //
    Thread::Thread(U32 (STDCALL* func)(void*), void* arg)
    {
        Start(func, arg);
    }


    //
    // ~Thread
    //
    Thread::~Thread()
    {
        Stop();
    }


    //
    // Start
    // 
    void Thread::Start(U32 (STDCALL* func)(void*), void* arg)
    {
        threadHandle = CreateThread(nullptr, 0, func, arg, 0, &threadID);
    }


    //
    // Stop
    //
    void Thread::Stop()
    {
        if (threadHandle)
        {
            // Wait for the thread to end
            WaitForSingleObject(threadHandle, INFINITE);
            threadHandle = nullptr;
        }
    }


    //
    // Clear
    //
    void Thread::Clear()
    {
        threadHandle = nullptr;
    }


    //
    // SetPriority
    //
    // Change the priority of the thread
    //
    void Thread::SetPriority(Priority priority)
    {
        SetThreadPriority(threadHandle, priority);
    }


    //
    // GetPriority
    //
    // Retrieve the priority of the thread
    Thread::Priority Thread::GetPriority()
    {
        return static_cast<Priority>(GetThreadPriority(threadHandle));
    }


    //
    // GetID
    //
    // Retrieve the ID of the thread
    //
    inline U32 Thread::GetId()
    {
        return (threadID);
    }


    //
    // IsAlive
    //
    // Determines if the thread is alive or not
    //
    Bool Thread::IsAlive()
    {
        U32 code;
        GetExitCodeThread(threadHandle, &code);
        return ((code == STILL_ACTIVE) ? TRUE : FALSE);
    }


    //
    // Suspend
    //
    // Suspends execution of the thread
    // Multiple suspends will require multiple resumes
    //
    Bool Thread::Suspend()
    {
        return ((SuspendThread(threadHandle) == 0xFFFFFFFF) ? FALSE : TRUE);
    }


    //
    // Resume
    //
    // Resumes execution of the thread
    //
    Bool Thread::Resume()
    {
        return ((ResumeThread(threadHandle) == 0xFFFFFFFF) ? FALSE : TRUE);
    }


    //
    // GetCurrentId
    //
    // Returns the id of the current thread
    //
    U32 Thread::GetCurrentId()
    {
        return (GetCurrentThreadId());
    }
}
