#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/EngineExport.h"
#include "Engine/Systems/ResourceManager/Importer/Importer.inl"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IGPUResourceFactory;

enum class LibraryExtPolicy : uint8_t
{
    FIXED,              // libraryFixedExtension is the extension (e.g. "nmesh", "nmat")
    PRESERVE_SOURCE,    // library file uses the source extension (e.g. AUDIO: .wav/.ogg)
    DIRECTORY_OF_STAGES // library "path" is a directory containing per-stage files (SHADER: .spv)
};

struct DisplayMetadata
{
    // RGBA, sRGB 0..1. Editor wraps this in ImVec4 at the call site so the
    // engine layer doesn't pull in ImGui.
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* icon = nullptr;
};

struct ResourceTypeDescriptor
{
    ResourceType type = ResourceType::UNKNOWN;
    const char* name = "Unknown";

    // Asset/library layout
    std::string libraryFolder;          // e.g. "Library/Meshes/"
    std::string libraryFixedExtension;  // used when policy == FIXED
    std::vector<std::string> sourceExtensions; // accepted Assets/ extensions, lowercase, no dot
    LibraryExtPolicy libExtPolicy = LibraryExtPolicy::FIXED;

    // Resource lifecycle
    MemoryTag memoryTag = MemoryTag::UNKNOWN;
    int  cleanupPriority = 0; // lower = destroyed first
    std::unique_ptr<Importer> importer;
    std::function<Resource* (uint32 uid)> createFn;
    std::function<void(Resource*)>        destroyFn;

    // Editor-facing metadata (kept neutral; editor adapts to ImGui types)
    DisplayMetadata display;
};

class NOUS_ENGINE_API ResourceTypeRegistry
{
public:
    ResourceTypeRegistry() = default;
    ~ResourceTypeRegistry();

    ResourceTypeRegistry(const ResourceTypeRegistry&) = delete;
    ResourceTypeRegistry& operator=(const ResourceTypeRegistry&) = delete;

    // Takes ownership of the descriptor.
    void Register(ResourceTypeDescriptor descriptor);

    [[nodiscard]] const ResourceTypeDescriptor* Get(ResourceType type) const;
    [[nodiscard]] ResourceTypeDescriptor* Get(ResourceType type);

    // Iteration helpers — both return non-owning pointers, valid until the
    // registry is destroyed.
    [[nodiscard]] std::vector<const ResourceTypeDescriptor*> All() const;
    [[nodiscard]] std::vector<const ResourceTypeDescriptor*> SortedByCleanupPriority() const;

    // Convenience lookups built on top of the descriptors.
    [[nodiscard]] ResourceType TypeFromExtension(const std::string& extension) const;

private:
    static constexpr size_t k_SlotCount = static_cast<size_t>(ResourceType::ALL_TYPES);
    std::array<std::unique_ptr<ResourceTypeDescriptor>, k_SlotCount> m_descriptors{};
};

// Implemented in RegisterBuiltinResourceTypes.cpp (Phase 2).
NOUS_ENGINE_API void RegisterBuiltinResourceTypes(ResourceTypeRegistry& registry);

// Singleton accessor — owned by Application; created before any module.
NOUS_ENGINE_API ResourceTypeRegistry& GetResourceTypeRegistry();
NOUS_ENGINE_API void                  SetResourceTypeRegistry(ResourceTypeRegistry* registry);
