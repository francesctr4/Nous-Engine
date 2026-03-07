#include "Engine/Modules/ModuleFileSystem/include/ModuleFileSystem.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <string>
#include "Engine/Core/FileSystem/FileSystem.h"
#include <filesystem>
#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_FILESYSTEM;

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

	// TODO: All this probably should be managed on Module Resource Manager, and in fact ModuleFileSystem is pretty worthless for now.
	if (!NOUS_FileManager::Exists("Library") ||
        !NOUS_FileManager::Exists("Library/Shaders") ||
        !NOUS_FileManager::Exists("Library/Meshes") ||
        !NOUS_FileManager::Exists("Library/Materials") ||
        !NOUS_FileManager::Exists("Library/Textures"))
	{
		CreateLibraryFolder();
		ImportDirectory("Assets");
	}

    NOUS_FileManager::CopyFile(R"(Assets\Settings\imgui.ini)", "imgui.ini");

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

void ModuleFileSystem::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            break;
    }
}
