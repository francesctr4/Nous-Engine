#include "ImporterMaterial.h"

#include "JsonFile.h"

#include "ResourceMaterial.h"

#include "MemoryManager.h"
#include "MetaFileData.inl"

//bool ImporterMaterial::Load(const char* path, ResourceMaterial* material)
//{
//    JsonFile jsonFile;
//
//    if (!jsonFile.LoadFromFile(path))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("name", materialConfig->name))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid name field");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("diffuse_color", materialConfig->diffuseColor))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("diffuse_map_name", materialConfig->diffuseMapName))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_name field");
//        return false;
//    }
//
//    return true;
//}
//
//bool ImporterMaterial::Load(const char* path, NOUS_MaterialSystem::MaterialConfig* materialConfig)
//{
//    JsonFile jsonFile;
//
//    if (!jsonFile.LoadFromFile(path)) 
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("name", materialConfig->name)) 
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid name field");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("diffuse_color", materialConfig->diffuseColor))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
//        return false;
//    }
//
//    if (!jsonFile.GetValue("diffuse_map_name", materialConfig->diffuseMapName))
//    {
//        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_name field");
//        return false;
//    }
//
//    return true;
//}
//
//bool ImporterMaterial::Save(const char* path, const Material& material)
//{
//    JsonFile jsonFile;
//
//    jsonFile.AppendValue("name", material.name);
//    jsonFile.AppendValue("diffuse_color", material.diffuseColor);
//    jsonFile.AppendValue("diffuse_map_name", material.diffuseMap.texture->name);
//
//    if (!jsonFile.SaveToFile(std::format("Library/Materials/{}.nmat", material.name).c_str())) 
//    {
//
//    }
//
//	return true;
//}

bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    // TODO
    
    JsonFile jsonFile;
    
	if (!jsonFile.LoadFromFile(metaFileData.assetsPath.c_str()))
	{
		NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
		return false;
	}

	//if (!jsonFile.GetValue("name", name))
	//{
	//	NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid name field");
	//	return false;
	//}

	//if (!jsonFile.GetValue("diffuse_color", materialConfig->diffuseColor))
	//{
	//	NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
	//	return false;
	//}

	//if (!jsonFile.GetValue("diffuse_map_name", materialConfig->diffuseMapName))
	//{
	//	NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_name field");
	//	return false;
	//}

	Resource* hola = new ResourceMaterial();

	hola->SetUID(77834);

    return Save(metaFileData, hola);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    // TODO
	const ResourceMaterial* material = static_cast<const ResourceMaterial*>(inResource);
	if (!material) return false;

	delete material;

    return true;
}

bool ImporterMaterial::Load(const MetaFileData& metaFileData, Resource* outResource)
{
    ResourceMaterial* material = static_cast<ResourceMaterial*>(outResource);
    if (!material) return false;

    // TODO: Load

    return true;
}

bool ImporterMaterial::Unload(Resource*& inResource)
{
	ResourceMaterial* material = static_cast<ResourceMaterial*>(inResource);
	if (!material) return false;

	// TODO: Unload

	return true;
}
