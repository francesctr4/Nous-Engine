#pragma once

#include "Globals.h"

// --------------- Vertex Struct --------------- //

struct Vertex3D
{
    float3 position;
    float3 color;
    float2 texCoord;

    static const uint16 ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex3D& other) const
    {
        return (position.Equals(other.position) && color.Equals(other.color) && texCoord.Equals(other.texCoord));
    }
};

struct Vertex2D
{
    float2 position;
    float3 color;
    float2 texCoord;

    static const uint16 ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex2D& other) const
    {
        return (position.Equals(other.position) && color.Equals(other.color) && texCoord.Equals(other.texCoord));
    }
};