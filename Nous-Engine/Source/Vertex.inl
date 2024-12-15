#pragma once

#include "Globals.h"

// --------------- Vertex Struct --------------- //

struct Vertex
{
    float3 position;
    float3 color;
    float2 texCoord;

    static const uint16 ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex& other) const
    {
        return (position.Equals(other.position) && color.Equals(other.color) && texCoord.Equals(other.texCoord));
    }
};