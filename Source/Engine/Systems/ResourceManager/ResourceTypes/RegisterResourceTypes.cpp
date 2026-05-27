#include "Engine/Systems/ResourceManager/TypeRegistry/TypeRegistry.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/ResourceManager/ImporterManager/Importer.inl"

// Importers
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceTexture/include/ImporterTexture.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceShader/include/ImporterShader.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceAudio/include/ImporterAudio.h"

// Resources
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceAudio/include/ResourceAudio.h"

namespace
{
    // Cleanup priorities — lower destroyed first.
    // Shaders go first (materials reference them), then materials (reference
    // textures + shaders), then leaf resources.
    constexpr int k_PrioShader   = 0;
    constexpr int k_PrioMaterial = 1;
    constexpr int k_PrioTexture  = 2;
    constexpr int k_PrioMesh     = 3;
    constexpr int k_PrioAudio    = 4;
}

void RegisterResourceTypes(TypeRegistry& registry)
{
    // ---------- SHADER ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::SHADER;
        d.name = "Shader";
        d.libraryFolder = "Library/Shaders/";
        d.libraryFixedExtension.clear();
        d.sourceExtensions = { "glsl", "spv" };
        d.libExtPolicy = LibraryExtPolicy::DIRECTORY_OF_STAGES;
        d.memoryTag = MemoryTag::RESOURCE_SHADER;
        d.cleanupPriority = k_PrioShader;
        d.importer = std::make_unique<ImporterShader>();
        d.createFn = [](uint32 uid) -> Resource* { return NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER, uid); };
        d.destroyFn = [](Resource* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_SHADER); };
        d.display.color[0] = 0.7f; d.display.color[1] = 0.2f; d.display.color[2] = 1.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- MATERIAL ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::MATERIAL;
        d.name = "Material";
        d.libraryFolder = "Library/Materials/";
        d.libraryFixedExtension = "nmat";
        d.sourceExtensions = { "nmat" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_MATERIAL;
        d.cleanupPriority = k_PrioMaterial;
        d.importer = std::make_unique<ImporterMaterial>();
        d.createFn = [](uint32 uid) -> Resource* { return NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL, uid); };
        d.destroyFn = [](Resource* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_MATERIAL); };
        d.display.color[0] = 0.8f; d.display.color[1] = 0.5f; d.display.color[2] = 0.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- TEXTURE ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::TEXTURE;
        d.name = "Texture";
        d.libraryFolder = "Library/Textures/";
        d.libraryFixedExtension = "png";
        d.sourceExtensions = { "png", "jpg", "jpeg", "tga" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_TEXTURE;
        d.cleanupPriority = k_PrioTexture;
        d.importer = std::make_unique<ImporterTexture>();
        d.createFn = [](uint32 uid) -> Resource* { return NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE, uid); };
        d.destroyFn = [](Resource* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_TEXTURE); };
        d.display.color[0] = 0.5f; d.display.color[1] = 0.8f; d.display.color[2] = 0.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- MESH ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::MESH;
        d.name = "Mesh";
        d.libraryFolder = "Library/Meshes/";
        d.libraryFixedExtension = "nmesh";
        d.sourceExtensions = { "fbx", "obj", "glb", "gltf", "nmesh" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_MESH;
        d.cleanupPriority = k_PrioMesh;
        d.importer = std::make_unique<ImporterMesh>();
        d.createFn = [](uint32 uid) -> Resource* { return NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH, uid); };
        d.destroyFn = [](Resource* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_MESH); };
        d.display.color[0] = 0.0f; d.display.color[1] = 0.8f; d.display.color[2] = 0.5f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- AUDIO ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::AUDIO;
        d.name = "Audio";
        d.libraryFolder = "Library/Audio/";
        d.libraryFixedExtension.clear();
        d.sourceExtensions = { "wav", "ogg" };
        d.libExtPolicy = LibraryExtPolicy::PRESERVE_SOURCE;
        d.memoryTag = MemoryTag::RESOURCE_AUDIO;
        d.cleanupPriority = k_PrioAudio;
        d.importer = std::make_unique<ImporterAudio>();
        d.createFn = [](uint32 uid) -> Resource* { return NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO, uid); };
        d.destroyFn = [](Resource* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_AUDIO); };
        d.display.color[0] = 0.93f; d.display.color[1] = 0.28f; d.display.color[2] = 0.60f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }
}
