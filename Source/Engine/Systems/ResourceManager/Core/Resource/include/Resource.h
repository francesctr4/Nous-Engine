#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "Engine/Systems/ResourceManager/Types/ResourceType.h"

#include <string>

enum class ResourceState : uint8_t
{
    UNLOADED,   // No data loaded
    CPU_READY,  // CPU data loaded; waiting for GPU upload
    GPU_READY   // Fully resident on GPU; safe to render
};

class Resource 
{
public:

	NOUS_ENGINE_API Resource();
	NOUS_ENGINE_API Resource(uint32 uID, ResourceType type);
	NOUS_ENGINE_API virtual ~Resource();

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

	[[nodiscard]] NOUS_ENGINE_API static std::string GetLibraryExtensionFromType(ResourceType type);
	[[nodiscard]] NOUS_ENGINE_API static ResourceType GetTypeFromExtension(const std::string& extension);
	[[nodiscard]] NOUS_ENGINE_API static std::string GetLibraryDirectoryFromType(ResourceType type);

private:

	ResourceState m_state          = ResourceState::UNLOADED;

	std::string m_name;
	uint32      m_uID              = 0;
	ResourceType m_type             = ResourceType::UNKNOWN;
	uint32      m_referenceCount   = 0;

	std::string m_assetsFilePath;
	std::string m_libraryFilePath;
};