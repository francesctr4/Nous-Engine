#include <ResourceManager/Import/ModelImport/ModelImport.h>

#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>
#include <ResourceManager/Import/ModelImport/ModelAssetPlan.h>
#include <ResourceManager/Import/ModelParser/ModelParser.h>
#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <ResourceManager/Types/ResourceSkeleton/ImporterSkeleton.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <algorithm>
#include <array>
#include <cctype>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

namespace nous::engine::resource_manager
{
    // Contract and reasoning live on the declaration in ModelImport.h.
    bool EnsureStub(const std::string& stubPath,
                    const std::string& sourceModelPath,
                    const std::string& clipName)
    {
        // NEVER REPLACING an existing stub is the rule .nmat already follows, and it
        // is what keeps UIDs stable across a re-import: the .meta sits beside the
        // stub and is keyed to it. Only the library binary is regenerated -- and the
        // one derived key below.
        if (nous::engine::filesystem::Exists(stubPath))
        {
            JsonObject existing = JsonFile::LoadFromFile(stubPath);

            // A stub that does not parse is LEFT ALONE. Reconciling means reading the
            // object and writing it back with one key changed, so a failed parse would
            // hand back an empty object and quietly reduce the file to a lone
            // `source` -- destroying the authored settings in the one situation where
            // the user most needs the file intact to see what went wrong.
            if (!existing.HasKey("source"))
            {
                NOUS_WARN_C(CURRENT_CHANNEL,
                    "ImportModel: stub '%s' could not be read and was left untouched; "
                    "delete it to have it regenerated.", stubPath.c_str());
                return true;
            }

            // Compared NORMALIZED, never as raw strings. Stubs on disk carry Windows
            // backslashes while a scanned assetsPath may use forward slashes, and a
            // raw comparison would call every stub in the project stale -- rewriting
            // all of them on every launch, for a difference that names the same file.
            // A separator flip is not a move.
            const std::string stale = existing.GetString("source");

            if (nous::engine::filesystem::NormalizePath(stale) ==
                nous::engine::filesystem::NormalizePath(sourceModelPath))
                return true;

            // Only the derived back-pointer moves. `clip` and a .nanim's authored
            // loop/speed are the user's and ride through untouched, which is the
            // whole reason this reads-modifies-writes instead of rebuilding.
            existing.Set("source", sourceModelPath);

            if (!JsonFile::SaveToFile(existing, stubPath))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: could not reconcile stub '%s'.",
                             stubPath.c_str());
                return false;
            }

            NOUS_INFO_C(CURRENT_CHANNEL,
                "ImportModel: stub '%s' pointed at '%s' and now points at '%s' "
                "(the model moved).",
                stubPath.c_str(), stale.c_str(), sourceModelPath.c_str());

            return true;
        }

        JsonObject stub;
        stub.Set("source", sourceModelPath);

        // The clip's ORIGINAL name, not the sanitized filename: the fallback
        // re-parse matches this against the aiScene, where "mixamo.com" still
        // has its dot.
        if (!clipName.empty()) stub.Set("clip", clipName);

        if (!JsonFile::SaveToFile(stub, stubPath))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: could not write stub '%s'.",
                         stubPath.c_str());
            return false;
        }

        return true;
    }

    bool IsModelExtension(const std::string_view extensionWithDot)
    {
        static constexpr std::array<std::string_view, 5> c_modelExtensions = {
            ".fbx", ".obj", ".gltf", ".glb", ".dae"
        };

        return std::ranges::any_of(c_modelExtensions,
            [extensionWithDot](const std::string_view candidate)
            {
                return std::ranges::equal(candidate, extensionWithDot,
                    [](const char a, const char b)
                    {
                        return std::tolower(static_cast<unsigned char>(a)) ==
                               std::tolower(static_cast<unsigned char>(b));
                    });
            });
    }

    bool ImportModel(const MetaFileData& modelMeta, const ModelImportContext& context)
    {
        auto model = ParseModel(modelMeta.assetsPath, context.resources);

        if (!model)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: %s", model.error().c_str());
            return false;
        }

        // A model with no geometry -- an anim-only export -- writes NO mesh binary
        // rather than an empty one.
        if (!model->submeshes.empty() && !ImporterMesh::SaveModel(modelMeta, *model))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: failed to write mesh binary for '%s'.",
                         modelMeta.assetsPath.c_str());
            return false;
        }

        if (!context.ensureMeta)
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "ImportModel: no meta callback — skeleton and clips for '%s' were not written.",
                modelMeta.assetsPath.c_str());
            return true;
        }

        const ModelAssetPlan plan = PlanModelAssets(*model, modelMeta.assetsPath);

        if (!plan.skeletonStubPath.empty() &&
            EnsureStub(plan.skeletonStubPath, modelMeta.assetsPath, ""))
        {
            MetaFileData skeletonMeta;
            if (context.ensureMeta(plan.skeletonStubPath, ResourceType::SKELETON, skeletonMeta))
            {
                if (ImporterSkeleton::SaveSkeleton(skeletonMeta, model->skeleton))
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "ImportModel: '%s' -> %zu bone(s).",
                                plan.skeletonStubPath.c_str(), model->skeleton.BoneCount());
                }
                else
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: failed to write '%s'.",
                                 skeletonMeta.libraryPath.c_str());
                }
            }
        }

        for (const PlannedClip& planned : plan.clips)
        {
            if (!EnsureStub(planned.stubPath, modelMeta.assetsPath, planned.clipName))
                continue;

            MetaFileData clipMeta;
            if (!context.ensureMeta(planned.stubPath, ResourceType::ANIMATION, clipMeta))
                continue;

            // clipIndex indexes the ORIGINAL clips array -- PlanModelAssets dropped
            // the channel-less ones without renumbering, precisely so this lookup
            // cannot land on a neighbour.
            const auto& clip = model->clips[planned.clipIndex];

            // Read back from the stub rather than defaulting: EnsureStub above left
            // an existing stub alone, so this is where a user's authored loop/speed
            // survives a re-import (or a deleted Library/). Defaulting here would
            // silently reset every clip's settings whenever the FBX was touched.
            const AnimationSettings settings =
                ImporterAnimation::ReadSettingsFromStub(planned.stubPath);

            if (ImporterAnimation::SaveClip(clipMeta, clip, settings))
            {
                NOUS_INFO_C(CURRENT_CHANNEL, "ImportModel: '%s' -> %zu channel(s), %.2fs.",
                            planned.stubPath.c_str(), clip.ChannelCount(),
                            static_cast<double>(clip.duration));
            }
            else
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "ImportModel: failed to write '%s'.",
                             clipMeta.libraryPath.c_str());
            }
        }

        return true;
    }
}
