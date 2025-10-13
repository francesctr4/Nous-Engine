#ifndef METAFILEDATA_INL
#define METAFILEDATA_INL

#include "Engine/Core/Globals.h"
#include "Engine/Systems/Resource Manager/Resource Types/Resource.h"

struct MetaFileData
{
    MetaFileData() : uid(0), resourceType(ResourceType::UNKNOWN) {}
    
	std::string name;
    UID uid;
    ResourceType resourceType;
    std::string assetsPath;
    std::string libraryPath;
};

#endif // METAFILEDATA_INL