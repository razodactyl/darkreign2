///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Game Orders
// 29-JUL-1998
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "orders_game.h"
#include "unitobj.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Orders
//
namespace Orders
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Game
    //
    namespace Game
    {
        ///////////////////////////////////////////////////////////////////////////////
        //
        // Internal Data
        //
        U32 Scatter::orderId;


        ///////////////////////////////////////////////////////////////////////////////
        //
        // Class Move
        //


        //
        // Generate
        //
        void Scatter::Generate(Player& player)
        {
            Data data;

            // Setup data structure
            data.Setup(orderId, player);

            Add(data, sizeof(Data), player.IsRoute());
        }


        //
        // Execute
        //
        U32 Scatter::Execute(const U8*, Player& player)
        {
            UnitObj::Scatter(player.GetSelectedList());

            return (sizeof(Data));
        }
    }
}
