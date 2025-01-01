#pragma once

#include "Globals.h"

using UID = uint32;

enum class ResourceType 
{
	MESH,
	MATERIAL,
	TEXTURE
};

class Resource 
{
public:

	std::string GetName() const;
	UID GetUID() const;
	ResourceType GetType() const;
	uint32 GetReferenceCount() const;

	std::string GetAssetsPath() const;
	std::string GetLibraryPath() const;

	static std::string GetStringFromType(ResourceType type);

public:

	std::string name;
	UID UID;
	ResourceType type;
	uint32 referenceCount;

	std::string assetsFilePath;
	std::string libraryFilePath;

	bool isLoaded;

};