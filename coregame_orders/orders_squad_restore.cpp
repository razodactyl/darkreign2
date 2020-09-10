///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Squad Orders
// 26-MAR-1999
//


///////////////////////////////////////////////////////////////////////////////
//
// Includes
//
#include "orders_squad.h"
#include "resolver.h"
#include "gameobjctrl.h"
#include "squadobj.h"

#include "tasks_squadrestore.h"


///////////////////////////////////////////////////////////////////////////////
//
// NameSpace Orders
//
namespace Orders
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // NameSpace Squad
    //
    namespace Squad
    {
        ///////////////////////////////////////////////////////////////////////////////
        //
        // Class Restore
        //

        U32 Restore::orderId;


        //
        // Generate
        //
        void Restore::Generate(Player& player, U32 squad, Modifier mod)
        {
            Data data;

            // Setup data structure
            data.Setup(orderId, player);

            // Pack the squad
            data.squad = squad;

            // Pack the modifier
            data.mod = mod;

            // Add the order
            Add(data, sizeof(Data), player.IsRoute());
        }


        //
        // Execute
        //
        U32 Restore::Execute(const U8* data, Player& player)
        {
            const Data* d = (Data*)data;

            // Resolve the squad
            if (SquadObj* squadObj = Resolver::Object<SquadObj, SquadObjType>(d->squad))
            {
                // Tell 'em to restore
                IssueTask(d->mod, squadObj, new Tasks::SquadRestore(squadObj), player);
            }

            return (sizeof(Data));
        }
    }
}
