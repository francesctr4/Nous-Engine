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

#include <ModuleBase/Module.h>
#include <EventSystem/IEventListener.h>
#include <FileWatcher/FileWatcher.h>
#include <EngineCore/EngineExport.h>
#include <Renderer/RendererTypes.h>
// RendererTypes.h only forward-declares Vertex3D; m_normalLines holds them by value.
#include <Utils/Math/Vertex.inl>

#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------
struct RenderPacket;
struct SceneRenderData;
class RendererFrontend;
class ResourceBase;
class IGPUResourceFactory;
class IImporterManager;

// The three faces of the resource system this module uses. It deliberately does
// NOT keep a ModuleResourceManager*: that handle exposes ~22 methods, most of
// them asset-pipeline concerns (ScanAndImportAssets, RegenerateLibrary,
// GetTypeRegistry, the scene manifest) that have no meaning inside a render
// loop. Application does the upcast at construction, so the concrete module is
// not named anywhere in this target -- one Modules/ -> Modules/ edge fewer.
class IResourceGpuSync;
class IResourceLoader;
class IRenderResourceProvider;

// Dependency Injection
class ModuleCamera3D;
class ModuleScene;
class ModuleWindow;

class ModuleRenderer3D : public Module, public IEventListener
{
public:
	// ---------------------------------------------------------------------
	// Constructor / Destructor
	// ---------------------------------------------------------------------
	explicit ModuleRenderer3D(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		ModuleWindow* moduleWindow,
		ModuleCamera3D* moduleCamera3D,
		IResourceGpuSync* resourceGpuSync,
		IResourceLoader* resourceLoader,
		IRenderResourceProvider* resourceProvider,
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

	// Re-register the shader FileWatcher when a .glsl file is moved inside the editor.
	// Must be called after the file has been physically moved to newPath.
	NOUS_ENGINE_API void UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath);

	// Register a newly created .glsl file with the shader FileWatcher so hot-reload picks it up.
	NOUS_ENGINE_API void WatchShaderFile(const std::string& path);

private:
	// ---------------------------------------------------------------------
	// Internal Methods
	// ---------------------------------------------------------------------
	[[nodiscard]] bool BuildRenderPacket(RenderPacket* packet, const SceneRenderData& sceneData);

	// Release old GPU data + re-upload freshly-reimported assets.
	// Drains TakeReadyAssetUploads() — called at the top of PreUpdate().
	void FlushPendingAssetUploads();

	// Reads Library/shader_manifest.json and loads built-in shaders via
	// CreateResourceFromLibrary — no .meta files required. The manifest itself is
	// written by ImportPipeline at the end of ScanAndImportAssets.
	void LoadShadersFromManifest();

private:
	// ---------------------------------------------------------------------
	// Members
	// ---------------------------------------------------------------------
	RendererFrontend* mRendererFrontend;
	RenderMode m_renderMode = RenderMode::EDITOR;
	bool mIsMinimized = false;

	// Accumulated seconds since app start; fed to GlobalUBO.time each frame.
	float m_totalTime = 0.f;

	// Dependency Injection
	ModuleWindow* mModuleWindow;
	ModuleCamera3D* mModuleCamera3D;
	ModuleScene* mModuleScene;

	// In practice all three are the same ModuleResourceManager, upcast by
	// Application; each names only the slice this module needs. See the note by
	// the forward declarations above.
	IResourceGpuSync*        mResourceGpuSync = nullptr;  // per-frame upload/release queues
	IResourceLoader*         mResourceLoader  = nullptr;  // built-in shader + material loads
	IRenderResourceProvider* mResourceProvider = nullptr; // fallback textures/materials

	// World-space AABBs computed each frame in the bounding-box loop.
	// Keyed by GameObject ID; consumed by BuildRenderPacket for frustum culling.
	std::unordered_map<uint32_t, std::pair<glm::vec3, glm::vec3>> mMeshAABBCache;

	// Scratch buffers for the normals visualization. Members rather than locals so
	// the de-interleaving SkinVertices requires costs one allocation, not one per
	// frame — the overlay rebuilds every frame while it is enabled.
	std::vector<glm::vec3>  m_normalScratchPos;
	std::vector<glm::vec3>  m_normalScratchNrm;
	std::vector<glm::uvec4> m_normalScratchIDs;
	std::vector<glm::vec4>  m_normalScratchWts;
	std::vector<glm::vec3>  m_normalScratchOutPos;
	std::vector<glm::vec3>  m_normalScratchOutNrm;

	// The built segments themselves, kept for their capacity.
	std::vector<Vertex3D>   m_normalLines;

	// Watches Assets/Shaders/*.glsl for changes and triggers hot reload.
	// Populated in Start() (EDITOR mode only); Poll() called in PreUpdate().
	FileWatcher  m_shaderWatcher;
	uint32_t     m_shaderWatchFrameCounter = 0;
};

#endif // NOUS_ENGINE_MODULE_RENDERER3D_H
