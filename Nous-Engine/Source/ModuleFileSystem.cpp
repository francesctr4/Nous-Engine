#include "ModuleFileSystem.h"
#include "ModuleResourceManager.h"

#include "FileManager.h"
#include "JsonFile.h"

#include "MathUtils.h"

#include <filesystem>

ModuleFileSystem::ModuleFileSystem(Application* app) : Module(app)
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
	
	if (!NOUS_FileManager::Exists("Library"))
	{
		CreateLibraryFolder();
		ImportDirectory("Assets");
	}

	return ret;
}

bool ModuleFileSystem::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

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

bool ModuleFileSystem::ImportDirectory(const std::string& directory)
{
	if (!NOUS_FileManager::Exists(directory))
	{
		NOUS_ERROR("Import Directory ERROR: Directory does not exist: %s", directory.c_str());
		return false;
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
	{
		if (std::filesystem::is_regular_file(entry))
		{
			App->resourceManager->ImportFile(entry.path().string());
		}
	}

	return true;
}