#include <AnimationSystem/Transform.h>

#include <glm/gtc/matrix_transform.hpp>

namespace nous::engine::animation_system
{
    glm::mat4 Transform::ToMatrix() const
    {
        const glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        const glm::mat4 r = glm::mat4_cast(rotation);
        const glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    Transform Interpolate(const Transform& a, const Transform& b, float t)
    {
        // Exact at the ends. A finished cross-fade must hand back its target pose
        // unmodified, and glm::slerp at t == 1 returns a normalized b, not b.
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;

        Transform result;
        result.position = glm::mix(a.position, b.position, t);
        result.scale    = glm::mix(a.scale,    b.scale,    t);

        // glm::slerp negates the target when dot(a, b) < 0, so this takes the short
        // arc. Pinned by t_Interpolate.SlerpTakesShortArc.
        result.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));

        return result;
    }
}
