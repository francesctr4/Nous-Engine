#ifndef VERTEX_INL
#define VERTEX_INL

#include <glm/glm.hpp>

// --------------- Vertex Struct --------------- //

struct Vertex3D
{
    glm::vec3 position;     // location 0
    glm::vec3 normal;       // location 1 — per-face / UV-split normal (lighting)
    glm::vec3 color;        // location 2
    glm::vec2 texCoord;     // location 3 — UV0: albedo, normal map, metallic/roughness, emissive
    glm::vec3 smoothNormal; // location 4 — position-welded smooth normal (outline extrusion)
    glm::vec4 tangent;      // location 5 — xyz: tangent direction, w: bitangent handedness sign (±1) for TBN
    glm::vec2 texCoord2;    // location 6 — UV1: lightmap / baked AO

    static const uint16_t ATTRIBUTE_COUNT = 7;

    bool operator==(const Vertex3D& other) const
    {
        return position == other.position &&
               normal == other.normal &&
               color == other.color &&
               texCoord == other.texCoord &&
               smoothNormal == other.smoothNormal &&
               tangent == other.tangent &&
               texCoord2 == other.texCoord2;
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