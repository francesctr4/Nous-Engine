#include "Engine/Core/Modules/ModuleFileSystem/include/ModuleFileSystem.h"
#include "Engine/Core/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Systems/File System/FileManager.h"
#include <filesystem>
#include "Engine/Core/Application.h"
#include "Engine/Systems/Logging System/Logger.h"

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

// Add this method to ModuleFileSystem.cpp
bool ModuleFileSystem::CompileShaders()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    // Find glslc executable
    std::string glslc_path;

#ifdef _WIN32
    // Try common Windows locations
    std::vector<std::string> windows_paths = {
            "C:/VulkanSDK/1.3.268.0/Bin/glslc.exe",
            "C:/VulkanSDK/1.3.275.0/Bin/glslc.exe", // Add more versions as needed
            "glslc.exe" // Try PATH
    };

    for (const auto& path : windows_paths) {
        if (NOUS_FileManager::Exists(path) || path == "glslc.exe") {
            glslc_path = path;
            break;
        }
    }
#else
    // Try common Unix/Linux/macOS locations
    std::vector<std::string> unix_paths = {
        "/usr/bin/glslc",
        "/usr/local/bin/glslc",
        "glslc" // Try PATH
    };

    for (const auto& path : unix_paths) {
        if (NOUS_FileManager::Exists(path) || path == "glslc") {
            glslc_path = path;
            break;
        }
    }
#endif

    if (glslc_path.empty()) {
        NOUS_ERROR("glslc compiler not found!");
        return false;
    }

    // Ensure Library/Shaders directory exists
    if (!NOUS_FileManager::CreateDirectory("Library/Shaders")) {
        NOUS_ERROR("Failed to create Library/Shaders directory");
        return false;
    }

    // Find all .glsl files in Assets/Shaders
    if (!NOUS_FileManager::Exists("Assets/Shaders")) {
        NOUS_WARN("Assets/Shaders directory not found");
        return true; // Not an error if no shaders exist
    }

    bool success = true;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("Assets/Shaders")) {
        if (std::filesystem::is_regular_file(entry) && entry.path().extension() == ".glsl") {
            std::string input_file = entry.path().string();
            std::string relative_path = std::filesystem::relative(entry.path(), "Assets/Shaders").string();

            // Replace .glsl with .spv
            std::string output_file = "Library/Shaders/" + relative_path;
            size_t pos = output_file.find_last_of('.');
            if (pos != std::string::npos) {
                output_file = output_file.substr(0, pos) + ".spv";
            }

            // Ensure output directory exists
            std::filesystem::path output_path(output_file);
            if (!NOUS_FileManager::CreateDirectory(output_path.parent_path().string())) {
                NOUS_ERROR("Failed to create directory: %s", output_path.parent_path().string().c_str());
                success = false;
                continue;
            }

            // Determine shader stage
            std::string shader_stage;
            if (relative_path.find(".vert.") != std::string::npos) {
                shader_stage = "vertex";
            } else if (relative_path.find(".frag.") != std::string::npos) {
                shader_stage = "fragment";
            } else if (relative_path.find(".comp.") != std::string::npos) {
                shader_stage = "compute";
            } else if (relative_path.find(".geom.") != std::string::npos) {
                shader_stage = "geometry";
            } else if (relative_path.find(".tesc.") != std::string::npos) {
                shader_stage = "tesscontrol";
            } else if (relative_path.find(".tese.") != std::string::npos) {
                shader_stage = "tesseval";
            } else {
                NOUS_WARN("Unknown shader stage for: %s", relative_path.c_str());
                continue;
            }

            // Build command
            std::string command = "\"" + glslc_path + "\" -fshader-stage=" + shader_stage +
                                  " \"" + input_file + "\" -o \"" + output_file + "\"";

            NOUS_DEBUG("Compiling shader: %s -> %s", input_file.c_str(), output_file.c_str());

            int result = std::system(command.c_str());
            if (result != 0) {
                NOUS_ERROR("Failed to compile shader: %s", input_file.c_str());
                success = false;
            }
        }
    }

    return success;
}

void ModuleFileSystem::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            break;
    }
}
