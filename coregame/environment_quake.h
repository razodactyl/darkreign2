///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Environment Quake
//
// 01-NOV-1999
//


#ifndef __ENVIRONMENT_QUAKE_H
#define __ENVIRONMENT_QUAKE_H


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
    // NameSpace Quake
    //
    namespace Quake
    {
        void Init();
        void Done();

        void ProcessCreate(FScope* fScope);

        Bool SetWorldMatrix(FamilyNode& node, const Matrix& matrix);

        void Simulate(F32 dt);
    }
}


#endif
