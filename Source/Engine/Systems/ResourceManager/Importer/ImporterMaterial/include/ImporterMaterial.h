#ifndef IMPORTERMATERIAL_H
#define IMPORTERMATERIAL_H

#include "Engine/Systems/ResourceManager/Importer/Importer.inl"
#include "Engine/EngineExport.h"

#include <string>

class ResourceMaterial;

struct ImporterMaterial : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    void Evict(Resource* resource) override;
    bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(Resource* resource, IGPUResourceFactory* gpu) override;

    static NOUS_ENGINE_API bool SaveMaterialToAssets(ResourceMaterial* material);

    // Creates a minimal .nmat file at assetPath with the built-in MaterialShader's
    // default uniforms (diffuseColor, emissiveColor, aoIntensity, normalStrength,
    // specularIntensity, shininessScale) and an empty
    // texture_maps array. Used by the Inspector's "Create Material Asset" flow to
    // seed a new on-disk material asset for a GameObject that is currently using
    // the default material. Caller is responsible for calling
    // ModuleResourceManager::ImportFile(assetPath) and CreateResource(assetPath)
    // afterwards to register the new material.
    static NOUS_ENGINE_API bool CreateNewMaterialFile(const std::string& assetPath);
};

#endif // IMPORTERMATERIAL_H