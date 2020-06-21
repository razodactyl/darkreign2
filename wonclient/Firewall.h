#pragma once
#include "MINTCLIENT.h"
#include "Errors.h"


namespace MINTCLIENT
{
    struct Firewall
    {
        struct Data
        {
        };

        struct Result : CommandResult
        {
        };

        struct DetectResult : Result
        {
        };
    };
}
