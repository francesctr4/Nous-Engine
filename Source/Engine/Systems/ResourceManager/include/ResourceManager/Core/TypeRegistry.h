#pragma once

#include <EngineCore/Globals.h>
#include <MemoryManager/MemoryManager.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/IImporter.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
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

struct TypeDescriptor
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

    // Owning handle to the importer. Always set (every type imports). For types
    // with a runtime resource object, the concrete importer also derives
    // IResourceImporter and `resourceImporter` below points at the same object;
    // for pipeline-only types (SCENE) `resourceImporter` stays null — the same
    // "no runtime object" signal as a null createFn/destroyFn.
    std::unique_ptr<IAssetImporter> importer;
    IResourceImporter* resourceImporter = nullptr; // non-owning alias into `importer`

    std::function<ResourceBase* (uint32 uid)> createFn;
    std::function<void(ResourceBase*)>        destroyFn;

    // Constructs the importer and wires both handles. `resourceImporter` is set
    // only when T derives IResourceImporter (compile-time trait, no RTTI), so the
    // two handles can never disagree. Prefer this over assigning `importer`
    // directly.
    template <typename T>
    void SetImporter()
    {
        auto owned = std::make_unique<T>();
        if constexpr (std::is_base_of_v<IResourceImporter, T>)
            resourceImporter = owned.get();
        importer = std::move(owned);
    }

    // True when changes to the source asset should trigger an async reimport.
    // Currently set for TEXTURE and MATERIAL; shaders run their own watcher,
    // mesh/audio imports are too expensive for live editing.
    bool hotReloadable = false;

    // Optional teardown-time replacement for `importer->Evict(res)`. Set this
    // for types that hold pointers to peer resources of lower cleanupPriority
    // (i.e. resources that get destroyed later in the same ClearResources
    // pass): the default Evict path would route through UnloadResource and
    // recurse into the half-torn-down ModuleResourceManager. The callback
    // should null the cross-resource pointers inline; destroyFn still runs
    // afterwards to release the type's own memory.
    std::function<void(ResourceBase*)> evictAtShutdown;

    // Editor-facing metadata (kept neutral; editor adapts to ImGui types)
    DisplayMetadata display;
};

class NOUS_ENGINE_API TypeRegistry
{
public:
    TypeRegistry() = default;
    ~TypeRegistry();

    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // Takes ownership of the descriptor.
    void Register(TypeDescriptor descriptor);

    [[nodiscard]] const TypeDescriptor* Get(ResourceType type) const;
    [[nodiscard]] TypeDescriptor* Get(ResourceType type);

    // Iteration helpers — both return non-owning pointers, valid until the
    // registry is destroyed.
    [[nodiscard]] std::vector<const TypeDescriptor*> All() const;
    [[nodiscard]] std::vector<const TypeDescriptor*> SortedByCleanupPriority() const;

    // Convenience lookups built on top of the descriptors.
    [[nodiscard]] ResourceType TypeFromExtension(const std::string& extension) const;

private:
    static constexpr size_t k_SlotCount = static_cast<size_t>(ResourceType::ALL_TYPES);
    std::array<std::unique_ptr<TypeDescriptor>, k_SlotCount> m_descriptors{};
};

// Implemented in RegisterResourceTypes.cpp (Phase 2).
NOUS_ENGINE_API void RegisterResourceTypes(TypeRegistry& registry);
