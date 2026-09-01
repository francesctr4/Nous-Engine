#include <AnimationSystem/Palette.h>

#include <glm/gtc/matrix_transform.hpp>

namespace nous::engine::animation_system
{
    bool BuildGlobals(const SkeletonData& skeleton, const Pose& localPose,
                      std::vector<glm::mat4>& outGlobals)
    {
        const size_t boneCount = skeleton.BoneCount();

        if (localPose.bones.size() != boneCount)   return false;
        if (skeleton.parents.size() != boneCount)  return false;
        if (!skeleton.IsTopologicallySorted())     return false;

        outGlobals.resize(boneCount);

        // The single forward loop the topological invariant buys. parents[i] < i, so
        // outGlobals[parent] is already final by the time bone i reads it -- no
        // recursion, no visited set, no second pass.
        for (size_t i = 0; i < boneCount; ++i)
        {
            const glm::mat4 local = localPose.bones[i].ToMatrix();
            const int parent = skeleton.parents[i];

            outGlobals[i] = parent < 0 ? local : outGlobals[parent] * local;
        }

        return true;
    }

    bool BuildPalette(const SkeletonData& skeleton, std::span<const glm::mat4> globals,
                      std::vector<glm::mat4>& outPalette, const glm::mat4& rootGlobalInverse)
    {
        const size_t boneCount = skeleton.BoneCount();

        if (globals.size() != boneCount)          return false;
        if (skeleton.offsets.size() != boneCount) return false;

        outPalette.resize(boneCount);

        for (size_t i = 0; i < boneCount; ++i)
        {
            // offsets[i] carries a vertex from mesh space into bone i's space at
            // bind; globals[i] puts it back where the bone is now. At bind pose the
            // two cancel to identity, which is what t_Palette pins.
            outPalette[i] = rootGlobalInverse * globals[i] * skeleton.offsets[i];
        }

        return true;
    }

    bool BuildPalette(const SkeletonData& skeleton, const Pose& localPose,
                      std::vector<glm::mat4>& scratchGlobals, std::vector<glm::mat4>& outPalette,
                      const glm::mat4& rootGlobalInverse)
    {
        if (!BuildGlobals(skeleton, localPose, scratchGlobals)) return false;

        return BuildPalette(skeleton, scratchGlobals, outPalette, rootGlobalInverse);
    }

    bool SkinVertices(std::span<const glm::mat4>  palette,
                      std::span<const glm::vec3>  inPositions,
                      std::span<const glm::vec3>  inNormals,
                      std::span<const glm::uvec4> boneIDs,
                      std::span<const glm::vec4>  boneWeights,
                      std::span<glm::vec3>        outPositions,
                      std::span<glm::vec3>        outNormals)
    {
        const size_t count = inPositions.size();

        if (inNormals.size()  != count) return false;
        if (boneIDs.size()    != count) return false;
        if (boneWeights.size()!= count) return false;
        if (outPositions.size() < count) return false;
        if (outNormals.size()   < count) return false;

        for (size_t v = 0; v < count; ++v)
        {
            const glm::uvec4& ids = boneIDs[v];
            const glm::vec4&  w   = boneWeights[v];

            glm::mat4 skin(0.0f);
            bool anyInfluence = false;

            for (int i = 0; i < 4; ++i)
            {
                const float weight = w[i];
                if (weight == 0.0f) continue;

                const uint32_t bone = ids[i];

                // An out-of-range index means the mesh's bone IDs and the skeleton
                // disagree -- the exact failure the spec's shared BuildSkeleton and
                // bone-name hash exist to prevent. Skip rather than read past the
                // palette; a limb that does not move is a far better symptom than a
                // heap overread.
                if (bone >= palette.size()) continue;

                skin += palette[bone] * weight;
                anyInfluence = true;
            }

            // Unweighted vertices (rigid geometry in a skinned mesh, or a mesh whose
            // bone data is all zeros because it has no skin) pass through unchanged.
            // Accumulating into a zero matrix would send them to the origin.
            if (!anyInfluence)
            {
                outPositions[v] = inPositions[v];
                outNormals[v]   = inNormals[v];
                continue;
            }

            outPositions[v] = glm::vec3(skin * glm::vec4(inPositions[v], 1.0f));

            // Upper 3x3 only, then renormalize. Correct for rigid and uniformly
            // scaled bones; non-uniform bone scale would need the inverse transpose.
            const glm::vec3 n = glm::mat3(skin) * inNormals[v];
            const float len = glm::length(n);
            outNormals[v] = len > 0.0f ? n / len : inNormals[v];
        }

        return true;
    }
}
