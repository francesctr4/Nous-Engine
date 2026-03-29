#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

#include <string>

using UID = uint32;

enum class ResourceState
{
    UNLOADED,   // No data loaded
    CPU_READY,  // CPU data loaded; waiting for GPU upload
    GPU_READY   // Fully resident on GPU; safe to render
};

enum class ResourceType 
{
	UNKNOWN = -1,

	MESH,
	MATERIAL,
	TEXTURE,
	SHADER,

	ALL_TYPES
};

class Resource 
{
public:

	NOUS_ENGINE_API Resource();
	NOUS_ENGINE_API Resource(UID uID, ResourceType type);
	NOUS_ENGINE_API virtual ~Resource();

	NOUS_ENGINE_API void SetName(std::string_view name);
	NOUS_ENGINE_API void SetUID(const UID& uid);
	NOUS_ENGINE_API void SetType(const ResourceType& rType);

	NOUS_ENGINE_API void SetAssetsPath(std::string_view assetsFilePath);
	NOUS_ENGINE_API void SetLibraryPath(std::string_view libraryFilePath);

	[[nodiscard]] NOUS_ENGINE_API std::string GetName() const;
	[[nodiscard]] NOUS_ENGINE_API UID GetUID() const;
	[[nodiscard]] NOUS_ENGINE_API ResourceType GetType() const;

	[[nodiscard]] NOUS_ENGINE_API std::string GetAssetsPath() const;
	[[nodiscard]] NOUS_ENGINE_API std::string GetLibraryPath() const;

	[[nodiscard]] NOUS_ENGINE_API uint32 GetReferenceCount() const;
	NOUS_ENGINE_API void IncreaseReferenceCount();
	NOUS_ENGINE_API void DecreaseReferenceCount();

	[[nodiscard]] NOUS_ENGINE_API bool IsValid() const;
	NOUS_ENGINE_API void Validate();
	NOUS_ENGINE_API void Invalidate();

	[[nodiscard]] NOUS_ENGINE_API ResourceState GetState() const;
	NOUS_ENGINE_API void SetState(ResourceState newState);

	[[nodiscard]] NOUS_ENGINE_API static int16 GetIndexFromType(const ResourceType& type);
	[[nodiscard]] NOUS_ENGINE_API static std::string GetLibraryExtensionFromType(ResourceType type);
	[[nodiscard]] NOUS_ENGINE_API static ResourceType GetTypeFromExtension(const std::string& extension);
	[[nodiscard]] NOUS_ENGINE_API static std::string GetAssetsDirectoryFromType(ResourceType type);
	[[nodiscard]] NOUS_ENGINE_API static std::string GetLibraryDirectoryFromType(ResourceType type);

private:

	bool m_valid;
	ResourceState m_state;

	std::string m_name;
	UID m_uID;
	ResourceType m_type;
	uint32 m_referenceCount;

	std::string m_assetsFilePath;
	std::string m_libraryFilePath;
};