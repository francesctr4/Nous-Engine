#pragma once

#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceType.h>

#include <functional>
#include <string>
#include <string_view>

class IResourceLoader;

namespace nous::engine::resource_manager
{
    // True for the extensions ParseModel can open (leading dot included, ".fbx").
    // Case-insensitive.
    //
    // Lives beside ModelParser because that unit owns assimp's format knowledge.
    // ImportPipeline must not grow a second copy of this list -- two lists drift,
    // and the failure mode is a model silently importing as a mesh with no
    // skeleton and no clips.
    [[nodiscard]] bool IsModelExtension(std::string_view extensionWithDot);

    struct ModelImportContext
    {
        // Handed to ParseModel, which uses it for the glTF material/texture
        // fan-out. Null disables that fan-out, so it must NOT be null in the
        // shipping path -- .nmat siblings would silently stop being generated.
        IResourceLoader* resources = nullptr;

        // "Give me this asset's MetaFileData, creating the .meta and assigning a UID
        // if it has none."
        //
        // A callback rather than a direct call into ImportPipeline: the pipeline
        // owns UID generation and the .meta format, and this unit must not become a
        // second place that knows either.
        std::function<bool(const std::string& assetsPath,
                           ResourceType       type,
                           MetaFileData&      outMeta)> ensureMeta;
    };

    // Imports ONE model file completely: parses it once, then fans out to whichever
    // importers the parse result calls for.
    //
    //   submeshes  -> ImporterMesh::SaveModel
    //   skeleton   -> stub + .meta + ImporterSkeleton::SaveSkeleton
    //   each clip  -> stub + .meta + ImporterAnimation::SaveClip
    //
    // ONE PARSE, ALWAYS. The rejected alternative -- each sibling importer parsing
    // for itself off its stub -- reads an 8.4 MB FBX 1 + 1 + N times (3 for a
    // Mixamo download, 22 for a 20-take DCC export). That is precisely the O(N)
    // re-parse the ModelParser pre-pass exists to delete, reintroduced one layer up.
    // The stub importers keep a re-parsing Import() only as a FALLBACK, for when a
    // stub outlives its library binary.
    //
    // Returns false only when the parse itself failed or the mesh write failed. A
    // sibling that cannot be written is logged and skipped: one broken clip must not
    // cost you the mesh.
    [[nodiscard]] bool ImportModel(const MetaFileData& modelMeta,
                                    const ModelImportContext& context);
}
