#pragma once

#include <ResourceManager/Import/ModelParser/ModelImportData.h>

#include <expected>
#include <string>

class IResourceLoader;

namespace nous::engine::resource_manager
{
    // Parses one model file (FBX / glTF / OBJ / DAE / ...) into engine types.
    //
    // THIS IS THE ENGINE'S ONLY ENTRY POINT INTO ASSIMP. ModelParser.cpp is the one
    // translation unit that includes <assimp/*>, the same compile-firewall shape as
    // RendererBackendFactory.cpp for Vulkan, AudioBackendFactory.cpp for miniaudio
    // and VideoDecoderBackendFactory.cpp for FFmpeg. Importers downstream of this
    // are serializers: they receive ModelImportData and never see an aiScene.
    //
    // `resources` is used only for the material/texture side-effect below and may
    // be null, in which case materialPaths comes back empty and submeshes fall back
    // to the default material at spawn time.
    //
    // SIDE EFFECT, inherited from ImporterMesh and deliberately kept here rather
    // than pushed down: for .gltf/.glb this pulls referenced textures into Assets/
    // and emits one sibling .nmat per assimp material. It is model-level work, and
    // the spec is right that it never belonged under a mesh-level name -- this is
    // where it now lives. Existing .nmat files are left alone so user edits survive.
    //
    // std::expected rather than a bool + out-param: the failure reason is worth
    // reporting at import time, matching the ShaderLoadResult modernization.
    [[nodiscard]] std::expected<ModelImportData, std::string>
    ParseModel(const std::string& assetsPath, IResourceLoader* resources);
}
