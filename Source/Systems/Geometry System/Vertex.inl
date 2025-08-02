#ifndef VERTEX_INL
#define VERTEX_INL

#include "Core/Globals.h"
#include "Includes/glmath.h"

// --------------- Vertex Struct --------------- //

struct Vertex3D
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;

    static const uint16 ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex3D& other) const
    {
        return position == other.position &&
               color == other.color &&
               texCoord == other.texCoord;
    }
};

struct Vertex2D
{
    glm::vec2 position;
    glm::vec3 color;
    glm::vec2 texCoord;

    static const uint16 ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex2D& other) const
    {
        return position == other.position &&
               color == other.color &&
               texCoord == other.texCoord;
    }
};

#endif // VERTEX_INL