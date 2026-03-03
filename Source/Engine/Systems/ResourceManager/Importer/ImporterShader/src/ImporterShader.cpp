#include "Engine/Systems/ResourceManager/Importer/ImporterShader/include/ImporterShader.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

bool ImporterShader::Import(const MetaFileData& metaFileData)
{
    Resource* tempShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    return Save(metaFileData, tempShader);
}

bool ImporterShader::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_SHADER);

    return true;
}

bool ImporterShader::Load(const std::string& libraryPath, Resource* outResource)
{
    ResourceShader* material = down_cast<ResourceShader*>(outResource);

    return true;
}

bool ImporterShader::Unload(Resource* inResource)
{
	ResourceShader* material = down_cast<ResourceShader*>(inResource);

	return true;
}