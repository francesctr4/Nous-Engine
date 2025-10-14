#ifndef VERTEX_INL
#define VERTEX_INL

#include <Engine/Core/Globals.h>

#include <glm/glm.hpp>

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