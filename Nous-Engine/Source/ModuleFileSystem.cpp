#include "ModuleFileSystem.h"

#include "FileManager.h"

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

	NOUS_FileManager::CreateDirectory("Library/Models");
	NOUS_FileManager::CreateDirectory("Library/Textures");

	return true;
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