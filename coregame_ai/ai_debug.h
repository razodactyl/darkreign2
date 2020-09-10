/////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// AI Debuging
//
// 17-MAY-1999
//

#ifndef __AI_DEBUG_H
#define __AI_DEBUG_H


//////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "ai.h"


/////////////////////////////////////////////////////////////////////////////
//
// NameSpace AI
//
namespace AI
{
    /////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Debug
    //
    namespace Debug
    {
        // Init
        void Init();

        // Done
        void Done();

        // Init Simulation
        void InitSimulation();

        // Done Simulation
        void DoneSimulation();


        // Show PlanEvaluation
        Bool ShowPlanEvaluation();
    }
}

#endif
