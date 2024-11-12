#pragma once

#include "Globals.h"

namespace Random 
{
    uint64 Generate();

    uint64 GenerateInterval(uint64 min, uint64 max);
}