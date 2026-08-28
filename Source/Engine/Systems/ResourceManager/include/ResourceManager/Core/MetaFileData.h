#pragma once

#include "ResourceBase.h"
#include <cstdint>

struct MetaFileData
{
    MetaFileData() : uid(0), resourceType(ResourceType::UNKNOWN) {}
    
	std::string name;
    uint32_t uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};