#include <AnimationSystem/RootMotion.h>

#include <AnimationSystem/Binding.h>
#include <AnimationSystem/Pose.h>

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace nous::engine::animation_system
{
    namespace
    {
        constexpr glm::vec3 c_up{ 0.0f, 1.0f, 0.0f };

        // Folds an angle difference into [-pi, pi]. Without this, a yaw crossing
        // the seam reads as a near-full-circle spin in the opposite direction.
        float WrapAngle(float radians)
        {
            constexpr float c_twoPi = glm::two_pi<float>();

            radians = std::fmod(radians + glm::pi<float>(), c_twoPi);
            if (radians < 0.0f) radians += c_twoPi;   // fmod keeps the sign
            return radians - glm::pi<float>();
        }

        glm::vec3 Horizontal(const glm::vec3& v) { return glm::vec3(v.x, 0.0f, v.z); }
    }

    float ExtractYaw(const glm::quat& rotation)
    {
        const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);

        // Straight up or down: no heading to speak of, and atan2(0,0) is 0 anyway.
        return std::atan2(forward.x, forward.z);
    }

    int ResolveRootBone(const AnimationBinding& binding)
    {
        int root = -1;
        for (const int bone : binding.channelToBone)
        {
            // -1 is a channel naming a bone this skeleton does not have: normal,
            // and it must not win the minimum.
            if (bone < 0) continue;
            if (root < 0 || bone < root) root = bone;
        }
        return root;
    }

    RootMotionDelta ComputeRootDelta(const Transform& previous,
                                     const Transform& current,
                                     const Transform& clipStart,
                                     const Transform& clipEnd,
                                     const bool       wrapped)
    {
        RootMotionDelta delta;

        if (!wrapped)
        {
            delta.translation = Horizontal(current.position - previous.position);
            delta.yaw = WrapAngle(ExtractYaw(current.rotation) - ExtractYaw(previous.rotation));
            return delta;
        }

        // Split the frame at the seam: previous -> end of clip, then start -> now.
        delta.translation = Horizontal((clipEnd.position - previous.position) +
                                       (current.position - clipStart.position));

        delta.yaw = WrapAngle(ExtractYaw(clipEnd.rotation) - ExtractYaw(previous.rotation)) +
                    WrapAngle(ExtractYaw(current.rotation) - ExtractYaw(clipStart.rotation));

        return delta;
    }

    void StripRootMotion(Pose& pose, const int rootBone, const Transform& bindLocal)
    {
        if (rootBone < 0 || static_cast<size_t>(rootBone) >= pose.bones.size()) return;

        Transform& root = pose.bones[rootBone];

        root.position.x = bindLocal.position.x;
        root.position.z = bindLocal.position.z;

        // Rotate the clip's yaw out and the bind's yaw in, leaving pitch and roll
        // untouched. Symmetric with the position rule above.
        const float correction = ExtractYaw(bindLocal.rotation) - ExtractYaw(root.rotation);
        root.rotation = glm::normalize(glm::angleAxis(correction, c_up) * root.rotation);
    }

    RootMotionDelta BlendRootDelta(const RootMotionDelta& a,
                                   const RootMotionDelta& b,
                                   float                  weight)
    {
        weight = glm::clamp(weight, 0.0f, 1.0f);

        RootMotionDelta out;
        out.translation = glm::mix(a.translation, b.translation, weight);
        out.yaw         = glm::mix(a.yaw, b.yaw, weight);
        return out;
    }
}
