#pragma once

#include <ResourceManager/Import/ModelParser/ModelImportData.h>

#include <string>
#include <vector>

// The PURE half of the model import: given everything a model file turned out to
// contain, decide which sibling assets it implies and what they are called.
//
// NO FILE I/O, NO ASSIMP, NO IMPORTERS -- the same line SkeletonBuild.h and
// ClipBuild.h draw, for the same reason. Every decision on this side has a filename
// trap or an off-by-one in it, and t_ResourceManager_ModelAssetPlan drives all of
// them from a hand-built ModelImportData. The executor on the other side of the
// line (ModelImport.h) is deliberately thin.
namespace nous::engine::resource_manager
{
    struct PlannedClip
    {
        // Assets/-relative path of the .nanim stub, a sibling of the model file:
        // "Assets/RumbaDancing_WithSkin@mixamo_com.nanim".
        std::string stubPath;

        // The clip's ORIGINAL, unsanitized name ("mixamo.com"). Written into the
        // stub so the fallback re-parse can still find the clip in the source model
        // after the filename has had its dots beaten out of it.
        std::string clipName;

        // Index into ModelImportData::clips -- the ORIGINAL array, before
        // channel-less clips were dropped. Carrying the index rather than
        // re-deriving it downstream is what stops a dropped clip shifting every
        // later clip onto the wrong data.
        size_t clipIndex = 0;
    };

    struct ModelAssetPlan
    {
        // Empty when the model has no skeleton.
        std::string              skeletonStubPath;
        std::vector<PlannedClip> clips;
    };

    // Replaces every character that is awkward in a filename with '_', and returns
    // "Animation" for an empty name.
    //
    // The dot is the one that matters in practice: Mixamo names its clip
    // "mixamo.com", so an unsanitized stub would be "Model@mixamo.com.nanim" -- an
    // asset whose extension parses as ".com".
    [[nodiscard]] std::string SanitizeClipFileName(const std::string& clipName);

    // modelAssetsPath is the Assets/-relative path of the model file; every stub is
    // planned as a sibling of it, in its own directory.
    //
    // CLIPS WITH ZERO CHANNELS ARE DROPPED, and that is not defensive coding.
    // Mixamo's skinned FBX carries a second AnimationStack ("Take 001") bound to an
    // empty layer -- a real 3.33s duration and no curve nodes at all. Assimp reports
    // it faithfully, and without this it becomes a phantom animation resource
    // sitting next to the real one.
    [[nodiscard]] ModelAssetPlan PlanModelAssets(const ModelImportData& model,
                                                  const std::string& modelAssetsPath);
}
