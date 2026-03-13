#ifndef VERTEX_INL
#define VERTEX_INL

#include <glm/glm.hpp>

// --------------- Vertex Struct --------------- //

struct Vertex3D
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoord;

    static const uint16_t ATTRIBUTE_COUNT = 4;

    bool operator==(const Vertex3D& other) const
    {
        return position == other.position &&
               normal == other.normal &&
               color == other.color &&
               texCoord == other.texCoord;
    }
};

struct Vertex2D
{
    glm::vec2 position;
    glm::vec3 color;
    glm::vec2 texCoord;

    static const uint16_t ATTRIBUTE_COUNT = 3;

    bool operator==(const Vertex2D& other) const
    {
        return position == other.position &&
               color == other.color &&
               texCoord == other.texCoord;
    }
};

#endif // VERTEX_INL