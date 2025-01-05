#pragma once

#include "Globals.h"

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

	Resource();
	Resource(UID uID, ResourceType type);
	virtual ~Resource();

	void SetName(const std::string& name);
	void SetUID(const UID& uid);
	void SetType(const ResourceType& rType);

	void SetAssetsPath(const std::string& assetsFilePath);
	void SetLibraryPath(const std::string& libraryFilePath);

	std::string GetName() const;
	UID GetUID() const;
	ResourceType GetType() const;

	std::string GetAssetsPath() const;
	std::string GetLibraryPath() const;

	uint32 GetReferenceCount() const;
	void IncreaseReferenceCount();
	void DecreaseReferenceCount();

	static int16 GetIndexFromType(const ResourceType& type);
	static std::string GetLibraryExtensionFromType(ResourceType type);
	static ResourceType GetTypeFromExtension(const std::string& extension);
	static std::string GetAssetsDirectoryFromType(ResourceType type);
	static std::string GetLibraryDirectoryFromType(ResourceType type);

private:

	std::string name;
	UID uID;
	ResourceType type;
	uint32 referenceCount;

	std::string assetsFilePath;
	std::string libraryFilePath;
};