#include "ModuleFileSystem.h"

// Wrapper around std::filesystem
#include <filesystem>

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

// -------------------------------------------------------------------------------- //

std::string ModuleFileSystem::GetAbsolutePath(const std::string& path) const 
{
	try 
	{
		return std::filesystem::absolute(path).string();
	}
	catch (const std::filesystem::filesystem_error& e) 
	{
		NOUS_ERROR("File system error getting absolute path: %s", e.what());
		return {};
	}
}

bool ModuleFileSystem::Exists(const std::string& path) const 
{
	return std::filesystem::exists(path);
}

bool ModuleFileSystem::IsDirectory(const std::string& path) const 
{
	return std::filesystem::is_directory(path);
}

std::string ModuleFileSystem::GetFilename(const std::string& path) const 
{
	return std::filesystem::path(path).filename().string();
}

std::string ModuleFileSystem::GetExtension(const std::string& path) const 
{
	return std::filesystem::path(path).extension().string();
}
