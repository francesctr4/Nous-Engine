#ifndef NOUS_ENGINE_RENDERER_FRONTEND_H
#define NOUS_ENGINE_RENDERER_FRONTEND_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"
#include "Engine/EngineExport.h"

#include <atomic>
#include <functional>
#include <glm/glm.hpp>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// Forward declarations
class RendererBackend;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;
struct IEditorOverlay;
class ModuleWindow;
class ModuleResourceManager;
class EventSystem;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

/**
 * @brief High-level rendering controller.
 *
 * Orchestrates frame rendering, render passes, and delegates GPU operations to RendererBackend.
 */
class RendererFrontend : public IGPUResourceFactory
{
public:
    NOUS_ENGINE_API RendererFrontend();
	NOUS_ENGINE_API ~RendererFrontend();

	// ---------------------------------------------------------------------
	// Dependency Injection (call before Initialize)
	// ---------------------------------------------------------------------
	NOUS_ENGINE_API void InjectDependencies(
		ModuleWindow* window,
		EventSystem* eventSystem,
		NOUS_Multithreading::NOUS_JobSystem* jobSystem,
		ModuleResourceManager* resourceManager);

	// ---------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------
	[[nodiscard]] NOUS_ENGINE_API bool Initialize(RendererBackendType backendType);
	NOUS_ENGINE_API void Shutdown();
	NOUS_ENGINE_API void ReleaseFrameResources() noexcept;
	NOUS_ENGINE_API void OnResized(uint16_t width, uint16_t height);

	// ---------------------------------------------------------------------
	// Rendering
	// ---------------------------------------------------------------------
	[[nodiscard]] NOUS_ENGINE_API enum FrameResult DrawFrame(RenderPacket* packet);

	// ---------------------------------------------------------------------
	// GPU Resource Management
	// ---------------------------------------------------------------------
	[[nodiscard]] NOUS_ENGINE_API bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture) override;
	NOUS_ENGINE_API void DestroyTexture(ResourceTexture* texture) override;

	[[nodiscard]] NOUS_ENGINE_API bool CreateMaterial(ResourceMaterial* material) override;
	NOUS_ENGINE_API void DestroyMaterial(ResourceMaterial* material) override;

	[[nodiscard]] NOUS_ENGINE_API bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices,
									  uint32_t indexCount, const uint32_t* indices, ResourceMesh* outGeometry) override;
	NOUS_ENGINE_API void DestroyGeometry(ResourceMesh* geometry) override;

	[[nodiscard]] NOUS_ENGINE_API bool CreateShader(ResourceShader* shader) override;
	NOUS_ENGINE_API void DestroyShader(ResourceShader* shader) override;

	// Queue a full reload of all currently-loaded shaders.
	// Safe to call from inside a renderpass (e.g. UI/menu callbacks) — the actual
	// reload is deferred and executed on the next call to FlushPendingReloads().
	NOUS_ENGINE_API void ReloadAllShaders();

	// Queue a material shader change to be processed at the start of the next PreUpdate.
	// Safe to call from the Inspector (ImGui callback) — the actual GPU work (DestroyMaterial +
	// CreateMaterial) happens between frames in FlushPendingReslots().
	NOUS_ENGINE_API void RequestMaterialShaderChange(ResourceMaterial* material,
	                                                 ResourceShader* newShader);

	// Drain the reslot queue: DestroyMaterial + reassign shader + CreateMaterial for each entry.
	// Called from ModuleRenderer3D::PreUpdate after FlushCompletedReloads.
	NOUS_ENGINE_API void FlushPendingReslots();

	// Execute any queued reload requests. Dispatches async compile jobs to the
	// JobSystem — returns immediately, no GPU work done here.
	// Call from ModuleRenderer3D::PreUpdate() before m_shaderWatcher.Poll().
	NOUS_ENGINE_API void FlushPendingReloads();

	// Apply GPU swaps for all compile jobs that have finished since last frame.
	// Must be called between frames (before BeginFrame). Executes vkDeviceWaitIdle
	// once for the whole batch, then calls ApplyCompiledShader for each ready shader.
	// Call from ModuleRenderer3D::PreUpdate() BEFORE FlushPendingReloads().
	NOUS_ENGINE_API void FlushCompletedReloads();

	// Recompile and rebuild a single shader identified by its asset path.
	// Returns false if no matching shader is found or if compilation fails.
	NOUS_ENGINE_API bool ReloadShaderByPath(const std::string& path);

	// ---------------------------------------------------------------------
	// Mouse Picking
	// ---------------------------------------------------------------------
	/**
	 * @brief Render the scene to a pick buffer and read back the object ID
	 *        at the given pixel coordinate.
	 * @param pixelX  X coordinate in framebuffer space.
	 * @param pixelY  Y coordinate in framebuffer space.
	 * @param projection  Camera projection matrix.
	 * @param view  Camera view matrix.
	 * @return The objectUID at (pixelX, pixelY), or 0 if nothing was hit.
	 */
	NOUS_ENGINE_API uint32_t PickObjectAt(int32_t pixelX, int32_t pixelY,
										  const glm::mat4& projection, const glm::mat4& view,
										  const std::vector<GeometryRenderData>& geometries);

	// ---------------------------------------------------------------------
	// Object Outlining
	// ---------------------------------------------------------------------
	/**
	 * @brief Sets the list of objects that should be rendered with an outline
	 *        effect during the next frame.
	 *
	 * This function is typically used by the editor or selection system to
	 * visually highlight selected objects. The renderer will apply the outline
	 * effect when rendering the scene, usually by performing an additional
	 * stencil-based rendering pass.
	 *
	 * Passing an empty list clears the current outline selection.
	 *
	 * @param selectedGeometries Vector of geometries that should be outlined.
	 * @param outlineSettings Outline settings used for the outline effect.
	 */
	NOUS_ENGINE_API bool SetOutlinedGeometries(
		const std::vector<GeometryRenderData>& selectedGeometries,
		const OutlineSettings& outlineSettings = OutlineSettings{});

	// ---------------------------------------------------------------------
	// Bounding Boxes
	// ---------------------------------------------------------------------
	/**
	 * @brief Sets the bounding boxes to be drawn during the next frame.
	 *        Each entry carries a pre-computed transform and a color so that
	 *        both AABB and OBB can be submitted together.
	 *        Passing an empty vector disables bounding box rendering.
	 */
	NOUS_ENGINE_API void SetBoundingBoxes(const std::vector<BoundingBoxData>& boxes);

	// ---------------------------------------------------------------------
	// Camera Frustums
	// ---------------------------------------------------------------------
	/**
	 * @brief Sets the camera frustums to be drawn in the Scene View each frame.
	 *        Each CameraFrustumData holds 8 world-space corners (near + far quads).
	 *        Passing an empty vector disables frustum rendering.
	 */
	NOUS_ENGINE_API void SetCameraFrustums(const std::vector<CameraFrustumData>& frustums);

	// ---------------------------------------------------------------------
	// Accessors
	// ---------------------------------------------------------------------
	NOUS_ENGINE_API void SetBackendType(RendererBackendType backendType) noexcept;
	[[nodiscard]] NOUS_ENGINE_API enum RendererBackendType GetBackendType() const noexcept;

	NOUS_ENGINE_API void SetEditorOverlay(IEditorOverlay* overlay);
	NOUS_ENGINE_API void SetRenderMode(RenderMode mode) noexcept;
	[[nodiscard]] NOUS_ENGINE_API RenderMode GetRenderMode() const noexcept;

private:
	// ---------------------------------------------------------------------
	// Internal helpers
	// ---------------------------------------------------------------------
	[[nodiscard]] FrameResult BeginFrame(float dt);
	[[nodiscard]] FrameResult EndFrame(float dt);

	[[nodiscard]] bool ExecuteRenderpass(RenderpassType pass, const std::function<void()>& drawCommands);

	void DrawEditor();

	// Dispatch a background compile job for the given shader path/pointer.
	// No-op if a compile for this path is already in-flight.
	void DispatchCompileJob(const std::string& path, ResourceShader* shader);

private:

	RendererBackend* mBackend;
	RendererBackendType mBackendType;
	RenderMode mRenderMode = RenderMode::EDITOR;

	// Cached dependencies — applied to the backend after Create() inside Initialize()
	ModuleWindow*                        m_window          = nullptr;
	EventSystem*                         m_eventSystem     = nullptr;
	NOUS_Multithreading::NOUS_JobSystem* m_jobSystem       = nullptr;
	ModuleResourceManager*               m_resourceManager = nullptr;

	IEditorOverlay* mEditorOverlay;

	// Deferred reload flag — set by ReloadAllShaders(), consumed by FlushPendingReloads().
	bool m_pendingReloadAll = false;

	// ── Material reslot queue ─────────────────────────────────────────────────
	// Queued by RequestMaterialShaderChange(); drained each PreUpdate before resource uploads.
	struct PendingReslot { ResourceMaterial* material; ResourceShader* newShader; };
	std::vector<PendingReslot> m_pendingReslots;
	std::mutex                 m_reslotMutex;

	// ── Async compile pipeline ────────────────────────────────────────────────
	// Worker threads push completed compile results here; main thread drains it
	// in FlushCompletedReloads() each PreUpdate.
	struct PendingGPUSwap
	{
	    ResourceShader*  shader;
	    ShaderLoadResult compileResult;
	};

	std::mutex                      m_swapQueueMutex;
	std::vector<PendingGPUSwap>     m_readySwaps;     // worker → main thread handoff
	std::unordered_set<std::string> m_inFlightPaths;  // paths with an active compile job
	std::atomic<int>                m_inFlightJobCount { 0 }; // used by Shutdown to drain

	// Outlined geometries — populated each frame by SetOutlinedGeometries().
	std::vector<GeometryRenderData> mOutlinedGeometries;
	OutlineSettings                 mOutlineSettings;

	// Bounding boxes — populated each frame by SetBoundingBoxes().
	std::vector<BoundingBoxData> mBoundingBoxes;

	// Camera frustums — populated each frame by SetCameraFrustums().
	std::vector<CameraFrustumData> mCameraFrustums;

};

#endif // NOUS_ENGINE_RENDERER_FRONTEND_H