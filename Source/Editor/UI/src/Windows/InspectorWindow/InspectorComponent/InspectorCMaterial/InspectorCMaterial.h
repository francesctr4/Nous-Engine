#ifndef NOUS_ENGINE_INSPECTORCMATERIAL_H
#define NOUS_ENGINE_INSPECTORCMATERIAL_H

#include <string>

class RendererFrontend;
class CMaterial;
struct ReflectedBinding;
class ResourceTexture;
class ResourceShader;
class ModuleResourceManager;
class GameObject;

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawDefaultMaterial(const GameObject* go, CMaterial& mat,
                                   ModuleResourceManager* rm, const std::string& saveDir);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawShaderSelector(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm);

void DrawDefaultShaderOption(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm);

void LoadAndDisplayShaderOptions(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm);

void ApplyShader(RendererFrontend* rendererFrontend, CMaterial& mat,
    ResourceShader* newEffective, ModuleResourceManager* rm);

void SyncUniforms(const CMaterial& mat, ResourceShader* shader);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

ResourceShader* GetEffectiveShader(CMaterial& mat, ModuleResourceManager* rm);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawShaderUniforms(const CMaterial& mat, ResourceShader* effectiveShader);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawTextureMaps(CMaterial& mat, ModuleResourceManager* rm, ResourceShader* effectiveShader);

void CMaterialHandleTextureMapsDragDrop(CMaterial& mat, ModuleResourceManager* rm, const ReflectedBinding* rb,
                                        const ResourceTexture* currentTex);
template<typename T>
std::string NormalizePath(const T* data);

bool IsTextureFile(std::string_view path);

void ApplyDroppedTexture(CMaterial& mat, ModuleResourceManager* rm, const ReflectedBinding* rb,
    const ResourceTexture* currentTex, const std::string& path);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

#endif //NOUS_ENGINE_INSPECTORCMATERIAL_H
