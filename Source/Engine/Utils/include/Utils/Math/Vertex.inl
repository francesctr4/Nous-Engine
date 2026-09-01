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

    // Skinning influences, up to 4 per vertex — the cap assimp's
    // aiProcess_LimitBoneWeights enforces (and renormalizes to) at import.
    // boneIDs index the ResourceSkeleton's bone array, NOT anything mesh-local.
    //
    // ZERO-INITIALIZED, and that is the design: an unskinned mesh writes zeros
    // rather than using a second vertex layout, so there is exactly one Vertex3D,
    // one stride and one pipeline vertex-input description in the engine. All-zero
    // weights mean "no influence", which the skinning path treats as a passthrough
    // (see SkinVertices in AnimationSystem/Palette.h). The cost is 32 bytes per
    // vertex on static geometry; the alternative is two vertex formats forever.
    glm::uvec4 boneIDs{ 0u };       // location 7
    glm::vec4  boneWeights{ 0.0f }; // location 8

    static const uint16_t ATTRIBUTE_COUNT = 9;

    bool operator==(const Vertex3D& other) const
    {
        return position == other.position &&
               normal == other.normal &&
               color == other.color &&
               texCoord == other.texCoord &&
               smoothNormal == other.smoothNormal &&
               tangent == other.tangent &&
               texCoord2 == other.texCoord2 &&
               boneIDs == other.boneIDs &&
               boneWeights == other.boneWeights;
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