#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace nous::engine::animation_system
{
    // One bone's keyframe tracks. TIMES ARE SPLIT FROM VALUES so that finding the
    // bracketing key touches only the time array, walking a dense run of floats instead of
    // striding over key structs it will not read. The three tracks are independent -- an
    // exporter is free to emit 60 rotation keys and 1 position key, and usually does.
    //
    // All times are in SECONDS: assimp's ticks are divided by ticksPerSecond at import and
    // the tick rate is deliberately not carried forward, or every downstream site would
    // have to remember to divide and the one that forgets plays at 24x.
    struct AnimChannel
    {
        std::string boneName;

        std::vector<float>     posTimes;
        std::vector<glm::vec3> posValues;

        std::vector<float>     rotTimes;
        std::vector<glm::quat> rotValues;

        std::vector<float>     scaleTimes;
        std::vector<glm::vec3> scaleValues;

        // Times and values must be the same length per track, and times non-decreasing.
        // Sampling assumes both; this is how an importer or a test proves it instead of
        // trusting the exporter.
        [[nodiscard]] bool IsConsistent() const;
    };
}
