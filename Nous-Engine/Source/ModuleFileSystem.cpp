#include "ModuleFileSystem.h"

#include "FileManager.h"
#include "JsonFile.h"
#include "MathUtils.h"

ModuleFileSystem::ModuleFileSystem(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleFileSystem::~ModuleFileSystem()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleFileSystem::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	bool ret = true;
	
	ret = CreateLibraryFolder();

	return ret;
}

bool ModuleFileSystem::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	//JsonFile jsonFile;

	//jsonFile.AppendValue("version", "0.1");
	//jsonFile.AppendValue("name", "test_material");
	//jsonFile.AppendValue("diffuse_color", float4(1.54f,1.0f,1.0f,1.0f));
	//jsonFile.AppendValue("diffuse_map_name", "paving");

	//jsonFile.SaveToFile("Library/test.json");

	return true;
}

bool ModuleFileSystem::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

bool ModuleFileSystem::CreateLibraryFolder()
{
	return NOUS_FileManager::CreateDirectory("Library") &&
		   NOUS_FileManager::CreateDirectory("Library/Shaders") &&
		   NOUS_FileManager::CreateDirectory("Library/Meshes") &&
	       NOUS_FileManager::CreateDirectory("Library/Materials") &&
	       NOUS_FileManager::CreateDirectory("Library/Textures");
}
