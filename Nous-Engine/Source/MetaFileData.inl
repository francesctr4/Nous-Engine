#pragma once

#include "Globals.h"
#include "Resource.h"

struct MetaFileData
{
    MetaFileData() : uid(0), resourceType(ResourceType::UNKNOWN) {}
    
	std::string name;
    UID uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};