#ifndef VERTEX_INL
#define VERTEX_INL

#include <glm/glm.hpp>

// --------------- Vertex Struct --------------- //

struct Vertex3D
{
    glm::vec3 position;   // location 0
    glm::vec3 normal;     // location 1  — per-face / UV-split normal (used for lighting)
    glm::vec3 color;      // location 2
    glm::vec2 texCoord;   // location 3
    glm::vec3 smoothNormal; // location 4 — position-welded smooth normal (used for outlining)

    static const uint16_t ATTRIBUTE_COUNT = 5;

    bool operator==(const Vertex3D& other) const
    {
        return position == other.position &&
               normal == other.normal &&
               color == other.color &&
               texCoord == other.texCoord &&
               smoothNormal == other.smoothNormal;
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