#ifndef RESOURCE_H
#define RESOURCE_H

#include <Engine/Core/Globals.h>
#include <Engine/Core/EngineExport.h>

using UID = uint32;

enum class ResourceType 
{
	UNKNOWN = -1,

	MESH,
	MATERIAL,
	TEXTURE,

	ALL_TYPES
};

class Resource 
{
public:

	NOUS_ENGINE_API Resource();
	NOUS_ENGINE_API Resource(UID uID, ResourceType type);
	NOUS_ENGINE_API virtual ~Resource();

	NOUS_ENGINE_API void SetName(const std::string& name);
	NOUS_ENGINE_API void SetUID(const UID& uid);
	NOUS_ENGINE_API void SetType(const ResourceType& rType);

	NOUS_ENGINE_API void SetAssetsPath(const std::string& assetsFilePath);
	NOUS_ENGINE_API void SetLibraryPath(const std::string& libraryFilePath);

	NOUS_ENGINE_API std::string GetName() const;
	NOUS_ENGINE_API UID GetUID() const;
	NOUS_ENGINE_API ResourceType GetType() const;

	NOUS_ENGINE_API std::string GetAssetsPath() const;
	NOUS_ENGINE_API std::string GetLibraryPath() const;

	NOUS_ENGINE_API uint32 GetReferenceCount() const;
	NOUS_ENGINE_API void IncreaseReferenceCount();
	NOUS_ENGINE_API void DecreaseReferenceCount();

	NOUS_ENGINE_API bool IsValid() const;
	NOUS_ENGINE_API void Validate();
	NOUS_ENGINE_API void Invalidate();

	NOUS_ENGINE_API static int16 GetIndexFromType(const ResourceType& type);
	NOUS_ENGINE_API static std::string GetLibraryExtensionFromType(ResourceType type);
	NOUS_ENGINE_API static ResourceType GetTypeFromExtension(const std::string& extension);
	NOUS_ENGINE_API static std::string GetAssetsDirectoryFromType(ResourceType type);
	NOUS_ENGINE_API static std::string GetLibraryDirectoryFromType(ResourceType type);

private:

	bool valid;

	std::string name;
	UID uID;
	ResourceType type;
	uint32 referenceCount;

	std::string assetsFilePath;
	std::string libraryFilePath;
};

#endif // RESOURCE_H