#pragma once

#include "Globals.h"
#include "Resource.h"

struct MetaFileData
{
	std::string name;
    UID uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};