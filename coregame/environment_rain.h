///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Environment Rain
//
// 26-JUL-1999
//


#ifndef __ENVIRONMENT_RAIN_H
#define __ENVIRONMENT_RAIN_H


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "fscope.h"

///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Environment
//
namespace Environment
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Rain
    //
    namespace Rain
    {
        void Init();
        void Done();

        void Process();
        void Render();

        void LoadInfo(FScope* fScope);
        void SaveInfo(FScope* fScope);

        void PostLoad();
    };
}


#endif
