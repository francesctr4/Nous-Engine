#pragma once

#include "Engine/Core/Globals.h"
#include "ResourceBase.h"

struct MetaFileData
{
    MetaFileData() : uid(0), resourceType(ResourceType::UNKNOWN) {}
    
	std::string name;
    uint32 uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};