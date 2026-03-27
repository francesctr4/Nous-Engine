#ifndef NOUS_ENGINE_MODULE_RENDERER3D_H
#define NOUS_ENGINE_MODULE_RENDERER3D_H

// -----------------------------------------------------------------------------
// ModuleRenderer3D
// -----------------------------------------------------------------------------
//
// The 3D Renderer Module is responsible for managing the rendering pipeline.
// It acts as a bridge between the engine and the chosen renderer backend
// (Vulkan, OpenGL, DirectX, etc.).
//
// Responsibilities:
//  - Initialize and shutdown the renderer frontend/backend.
//  - Collect and build render packets each frame.
//  - Handle window resize and other render-related events.
// -----------------------------------------------------------------------------

#include "Engine/Modules/Module.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/EngineExport.h"
#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Renderer/IGPUResourceFactory.h"

#include <glm/glm.hpp>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------
struct RenderPacket;
struct SceneRenderData;
class RendererFrontend;
class Resource;

// Dependency Injection
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleWindow;

class ModuleRenderer3D : public Module, public IEventListener
{
public:
	// ---------------------------------------------------------------------
	// Constructor / Destructor
	// ---------------------------------------------------------------------
	explicit ModuleRenderer3D(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode,
		ModuleWindow* moduleWindow,
		ModuleCamera3D* moduleCamera3D,
		ModuleResourceManager* moduleResourceManager,
		ModuleScene* moduleScene
		);
	~ModuleRenderer3D() override;

	// ---------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------
	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	// ---------------------------------------------------------------------
	// Events
	// ---------------------------------------------------------------------
	void OnEvent(const Event& event) override;

	// ---------------------------------------------------------------------
	// Accessors
	// ---------------------------------------------------------------------
	[[nodiscard]] NOUS_ENGINE_API RendererFrontend*      GetRendererFrontend() const;
	[[nodiscard]] NOUS_ENGINE_API IGPUResourceFactory*   GetGPUFactory()       const;

	// Toggle frustum culling against the game camera. Default: disabled.
	bool frustumCullingEnabled = false;

	NOUS_ENGINE_API void SetRenderMode(RenderMode mode) noexcept;

private:
	// ---------------------------------------------------------------------
	// Internal Methods
	// ---------------------------------------------------------------------
	[[nodiscard]] bool BuildRenderPacket(RenderPacket* packet, const SceneRenderData& sceneData);

	// Writes Library/shader_manifest.json with the UIDs and library paths of the
	// built-in shaders so GAME mode can load them without reading .meta files.
	void WriteShaderManifest(const Resource* matShader, const Resource* bgShader) const;

	// Reads Library/shader_manifest.json and loads built-in shaders via
	// CreateResourceFromLibrary — no .meta files required.
	void LoadShadersFromManifest();

private:
	// ---------------------------------------------------------------------
	// Members
	// ---------------------------------------------------------------------
	RendererFrontend* mRendererFrontend;
	RenderMode m_renderMode = RenderMode::EDITOR;
	bool mIsMinimized = false;

	// Dependency Injection
	ModuleWindow* mModuleWindow;
	ModuleCamera3D* mModuleCamera3D;
	ModuleResourceManager* mModuleResourceManager;
	ModuleScene* mModuleScene;

	// World-space AABBs computed each frame in the bounding-box loop.
	// Keyed by GameObject ID; consumed by BuildRenderPacket for frustum culling.
	std::unordered_map<uint32_t, std::pair<glm::vec3, glm::vec3>> mMeshAABBCache;
};

#endif // NOUS_ENGINE_MODULE_RENDERER3D_H
