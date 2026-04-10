#ifndef METAFILEDATA_INL
#define METAFILEDATA_INL

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"

struct MetaFileData
{
    MetaFileData() : uid(0), resourceType(ResourceType::UNKNOWN) {}
    
	std::string name;
    uint32 uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};

#endif // METAFILEDATA_INL