#include "Engine/Modules/ModuleFileSystem/include/ModuleFileSystem.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <string>
#include "Engine/Core/FileSystem/FileSystem.h"
#include <filesystem>
#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/Tests/ShaderMockShaders.h"

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
	
	if (!NOUS_FileManager::Exists("Library") ||
        !NOUS_FileManager::Exists("Library/Shaders") ||
        !NOUS_FileManager::Exists("Library/Meshes") ||
        !NOUS_FileManager::Exists("Library/Materials") ||
        !NOUS_FileManager::Exists("Library/Textures"))
	{
		CreateLibraryFolder();
        CompileShaders();
		ImportDirectory("Assets");
	}

    NOUS_FileManager::CopyFile(R"(Assets\Settings\imgui.ini)", "imgui.ini");

	return ret;
}

bool ModuleFileSystem::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	/* TEMP */
	Test_CompileShader();
	/* END TEMP */

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

// Handle on resource manager -> manage shaders as a resource.
bool ModuleFileSystem::CompileShaders()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	namespace fs = std::filesystem;

	const fs::path inRoot  = fs::path("Assets/Shaders");
	const fs::path outRoot = fs::path("Library/Shaders");

	if (!fs::exists(inRoot)) {
		NOUS_ERROR("Shader input directory not found: %s", inRoot.string().c_str());
		return false;
	}

	bool allOk = true;
	uint32_t compiledCount = 0;
	uint32_t skippedCount  = 0;

	auto isShaderStageFile = [](const fs::path& p) -> bool {
		const std::string s = p.string();
		return
				(s.size() >= 10 && s.ends_with(".vert.glsl")) ||
				(s.size() >= 10 && s.ends_with(".frag.glsl")) ||
				(s.size() >= 10 && s.ends_with(".comp.glsl")) ||
				(s.size() >= 10 && s.ends_with(".geom.glsl")) ||
				(s.size() >= 10 && s.ends_with(".tesc.glsl")) ||
				(s.size() >= 10 && s.ends_with(".tese.glsl"));
	};

	for (const auto& entry : fs::recursive_directory_iterator(inRoot)) {
		if (!entry.is_regular_file())
			continue;

		const fs::path inPath = entry.path();

		// Only compile files matching your stage pattern (*.vert.glsl, *.frag.glsl, etc.)
		if (!isShaderStageFile(inPath)) {
			++skippedCount;
			continue;
		}

		// Build output path: Library/Shaders/<relative_path>.spv
		fs::path rel = fs::relative(inPath, inRoot);      // e.g. BuiltIn/UI.vert.glsl
		fs::path outPath = outRoot / rel;                 // e.g. Library/Shaders/BuiltIn/UI.vert.glsl
		outPath.replace_extension(".spv");                // becomes .../UI.vert.spv (NOTE: only replaces ".glsl")

		// If you want ".vert.glsl" -> ".vert.spv" exactly:
		// replace_extension(".spv") turns ".glsl" into ".spv" which is what you want.

		const bool ok = NOUS_ShaderSystem::CompileGlslFileToSpirvFile(inPath.string(), outPath.string(),
				/*optimize=*/true,
				/*debugInfo=*/false);

		if (!ok) {
			NOUS_ERROR("Shader compilation failed: %s -> %s",
					   inPath.string().c_str(), outPath.string().c_str());
			allOk = false;
		} else {
			NOUS_DEBUG_C(CURRENT_CHANNEL, "[%s] Shader compiled: %s -> %s", __FUNCTION__,
					  inPath.string().c_str(), outPath.string().c_str());
			++compiledCount;
		}
	}

	NOUS_INFO("Shader compilation finished. Compiled=%u, Skipped=%u, Success=%s",
			  compiledCount, skippedCount, allOk ? "true" : "false");

	return allOk;
}

void ModuleFileSystem::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            break;
    }
}
