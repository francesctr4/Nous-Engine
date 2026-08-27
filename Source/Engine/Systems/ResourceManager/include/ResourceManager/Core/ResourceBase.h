#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include <ResourceManager/Types/ResourceType.h>

#include <atomic>
#include <string>

enum class ResourceState : uint8_t
{
    UNLOADED,   // No data loaded
    CPU_READY,  // CPU data loaded; waiting for GPU upload
    GPU_READY   // Fully resident on GPU; safe to render
};

class ResourceBase
{
public:

	NOUS_ENGINE_API ResourceBase();
	NOUS_ENGINE_API ResourceBase(uint32 uID, ResourceType type);
	NOUS_ENGINE_API virtual ~ResourceBase();

	NOUS_ENGINE_API void SetName(std::string_view name);
	NOUS_ENGINE_API void SetUID(uint32 uid);
	NOUS_ENGINE_API void SetType(ResourceType rType);

	NOUS_ENGINE_API void SetAssetsPath(std::string_view assetsFilePath);
	NOUS_ENGINE_API void SetLibraryPath(std::string_view libraryFilePath);

	[[nodiscard]] NOUS_ENGINE_API std::string GetName() const;
	[[nodiscard]] NOUS_ENGINE_API uint32 GetUID() const;
	[[nodiscard]] NOUS_ENGINE_API ResourceType GetType() const;

	[[nodiscard]] NOUS_ENGINE_API std::string GetAssetsPath() const;
	[[nodiscard]] NOUS_ENGINE_API std::string GetLibraryPath() const;

	[[nodiscard]] NOUS_ENGINE_API uint32 GetReferenceCount() const;
	NOUS_ENGINE_API void IncreaseReferenceCount();
	NOUS_ENGINE_API void DecreaseReferenceCount();

	// True once the resource has finished its CPU-side load and is safe for
	// dependent systems to reference. Equivalent to GetState() != UNLOADED.
	[[nodiscard]] NOUS_ENGINE_API bool IsLoaded() const;

	[[nodiscard]] NOUS_ENGINE_API ResourceState GetState() const;
	NOUS_ENGINE_API void SetState(ResourceState newState);

private:

	ResourceState m_state          = ResourceState::UNLOADED;

	std::string m_name;
	uint32      m_uID              = 0;
	ResourceType m_type             = ResourceType::UNKNOWN;

	// Atomic: resources are ref-counted from job-system workers (several at once
	// via PreloadSceneResourcesAsync) while the editor reads the count to display
	// it. A plain uint32 lost increments under concurrent load — an undercount
	// frees a resource that is still in use. ThreadSanitizer flagged both halves
	// of this on 2026-08-22 (ResourceBase.cpp:59 vs :64).
	std::atomic<uint32> m_referenceCount{0};

	std::string m_assetsFilePath;
	std::string m_libraryFilePath;
};