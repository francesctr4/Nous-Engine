#include <ResourceManager/Import/ModelImport/ModelAssetPlan.h>

#include <unordered_map>

namespace nous::engine::resource_manager
{
    namespace
    {
        // Everything before the final '.', keeping any directory prefix:
        // "Assets/Characters/Hero.fbx" -> "Assets/Characters/Hero".
        //
        // A dot inside a DIRECTORY name ("Assets/v1.0/Hero.fbx") cannot be mistaken
        // for the extension, because a dot that falls before the last separator is
        // rejected.
        std::string StripExtension(const std::string& path)
        {
            const size_t lastSlash = path.find_last_of("/\\");
            const size_t lastDot   = path.find_last_of('.');

            if (lastDot == std::string::npos) return path;
            if (lastSlash != std::string::npos && lastDot < lastSlash) return path;

            return path.substr(0, lastDot);
        }
    }

    std::string SanitizeClipFileName(const std::string& clipName)
    {
        if (clipName.empty()) return "Animation";

        std::string out;
        out.reserve(clipName.size());

        for (const char c : clipName)
        {
            // '@' joins the model stem to the clip name, so it must not also appear
            // inside one. Control characters and spaces go too, which keeps the
            // filename usable from a shell.
            const bool awkward = c == '.' || c == '/' || c == '\\' || c == ':' ||
                                 c == '*' || c == '?' || c == '"'  || c == '<' ||
                                 c == '>' || c == '|' || c == '@'  ||
                                 static_cast<unsigned char>(c) <= ' ';

            out += awkward ? '_' : c;
        }

        return out;
    }

    ModelAssetPlan PlanModelAssets(const ModelImportData& model,
                                    const std::string& modelAssetsPath)
    {
        ModelAssetPlan plan;
        const std::string stem = StripExtension(modelAssetsPath);

        if (model.HasSkeleton())
            plan.skeletonStubPath = stem + ".nskel";

        // Uniqueness is enforced on the SANITIZED name, not the original: sanitizing
        // can collapse two distinct clip names onto one filename ("a.b" and "a b"
        // both become "a_b"), and that collision is invisible in the originals.
        std::unordered_map<std::string, int> used;

        for (size_t i = 0; i < model.clips.size(); ++i)
        {
            const auto& clip = model.clips[i];

            if (clip.channels.empty()) continue;

            std::string fileName = SanitizeClipFileName(clip.name);

            if (const int previous = used[fileName]++; previous > 0)
                fileName += "_" + std::to_string(previous);

            PlannedClip planned;
            planned.stubPath  = stem + "@" + fileName + ".nanim";
            planned.clipName  = clip.name;
            planned.clipIndex = i;

            plan.clips.push_back(std::move(planned));
        }

        return plan;
    }
}
