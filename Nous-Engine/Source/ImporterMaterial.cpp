#include "ImporterMaterial.h"

#include "JsonFile.h"

bool ImporterMaterial::Load(const char* path, ResourceMaterial* material)
{
    /*JsonFile jsonFile;

    if (!jsonFile.LoadFromFile(path))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
        return false;
    }

    if (!jsonFile.GetValue("name", materialConfig->name))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid name field");
        return false;
    }

    if (!jsonFile.GetValue("diffuse_color", materialConfig->diffuseColor))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
        return false;
    }

    if (!jsonFile.GetValue("diffuse_map_name", materialConfig->diffuseMapName))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_name field");
        return false;
    }*/

    return true;
}

bool ImporterMaterial::Load(const char* path, NOUS_MaterialSystem::MaterialConfig* materialConfig)
{
    JsonFile jsonFile;

    if (!jsonFile.LoadFromFile(path)) 
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
        return false;
    }

    if (!jsonFile.GetValue("name", materialConfig->name)) 
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid name field");
        return false;
    }

    if (!jsonFile.GetValue("diffuse_color", materialConfig->diffuseColor))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
        return false;
    }

    if (!jsonFile.GetValue("diffuse_map_name", materialConfig->diffuseMapName))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_name field");
        return false;
    }

    return true;
}

bool ImporterMaterial::Save(const char* path, const Material& material)
{
    //JsonFile jsonFile;

    //jsonFile.AppendValue("name", material.name);
    //jsonFile.AppendValue("diffuse_color", material.diffuseColor);
    //jsonFile.AppendValue("diffuse_map_name", material.diffuseMap.texture->name);

    //if (!jsonFile.SaveToFile(std::format("Library/Materials/{}.nmat", material.name).c_str())) 
    //{

    //}

	return true;
}
