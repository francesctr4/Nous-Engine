#ifndef NOUS_ENGINE_FRUSTUM_CULLING_H
#define NOUS_ENGINE_FRUSTUM_CULLING_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace FrustumCulling
{
    // Six frustum planes as (a, b, c, d) where the plane equation is ax + by + cz + d = 0.
    // A world-space point P is on the inside of a plane when: dot(plane.xyz, P) + plane.w >= 0.
    struct FrustumPlanes
    {
        glm::vec4 planes[6]; // left, right, bottom, top, near, far
    };

    // Extract the six frustum planes from a combined view-projection matrix
    // using the Gribb-Hartmann method.
    // Assumes GLM clip-space convention (depth range [-1, 1] from glm::perspective).
    inline FrustumPlanes ExtractFrustumPlanes(const glm::mat4& vp)
    {
        // Transpose so m[i] gives mathematical row i, enabling direct row arithmetic.
        const glm::mat4 m = glm::transpose(vp);

        FrustumPlanes f;
        f.planes[0] = m[3] + m[0]; // left
        f.planes[1] = m[3] - m[0]; // right
        f.planes[2] = m[3] + m[1]; // bottom
        f.planes[3] = m[3] - m[1]; // top
        f.planes[4] = m[3] + m[2]; // near
        f.planes[5] = m[3] - m[2]; // far
        return f;
    }

    // Test a world-space axis-aligned bounding box against the frustum.
    // Returns true  if the AABB is fully or partially inside (should be rendered).
    // Returns false if the AABB is completely outside any single frustum plane (cull it).
    //
    // Uses the p-vertex (positive vertex) test: for each plane, pick the AABB corner
    // that lies furthest in the plane's normal direction. If even that corner is
    // outside the plane, the entire box is outside.
    inline bool IsAABBVisible(const FrustumPlanes& frustum,
                               const glm::vec3&    worldMin,
                               const glm::vec3&    worldMax)
    {
        for (const glm::vec4& plane : frustum.planes)
        {
            // Select the AABB vertex most aligned with the plane normal (p-vertex).
            const glm::vec3 pVertex(
                plane.x >= 0.0f ? worldMax.x : worldMin.x,
                plane.y >= 0.0f ? worldMax.y : worldMin.y,
                plane.z >= 0.0f ? worldMax.z : worldMin.z
            );

            if (plane.x * pVertex.x + plane.y * pVertex.y + plane.z * pVertex.z + plane.w < 0.0f)
                return false; // Completely outside this plane — cull.
        }
        return true; // Intersects or is fully inside all planes — keep.
    }

} // namespace FrustumCulling

#endif // NOUS_ENGINE_FRUSTUM_CULLING_H
