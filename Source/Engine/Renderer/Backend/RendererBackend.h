#ifndef NOUS_ENGINE_RENDERER_BACKEND_H
#define NOUS_ENGINE_RENDERER_BACKEND_H

#include "Engine/Renderer/RendererTypes.h"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;

/**
 * @brief Bridge layer between RendererFrontend and the active renderer implementation.
 *
 * Delegates all rendering operations to the selected backend (Vulkan, OpenGL, etc.)
 * through the IRendererBackend interface.
 */
class RendererBackend
{
public:

	RendererBackend();
	virtual ~RendererBackend();

	// ─────────────────────────────── Dependency Injection ────────────────────
	void InjectDependencies(
		EventSystem* eventSystem,
		NOUS_Multithreading::NOUS_JobSystem* jobSystem,
		ModuleWindow* window,
		ModuleResourceManager* resourceManager);

	// ─────────────────────────────── Lifecycle ───────────────────────────────
	[[nodiscard]] bool Create(RendererBackendType type);
	void Destroy();

	[[nodiscard]] bool Initialize();
	void Shutdown();
	void SetRenderMode(RenderMode mode) noexcept;
	void ReleaseFrameResources() noexcept;

	void Resized(uint16_t width, uint16_t height);

	// ─────────────────────────────── Frame Lifecycle ─────────────────────────
	[[nodiscard]] FrameResult BeginFrame(float dt);
	[[nodiscard]] FrameResult EndFrame(float dt);

	// ─────────────────────────────── Renderpasses ────────────────────────────
	[[nodiscard]] bool BeginRenderpass(RenderpassType renderpassID);
	[[nodiscard]] bool EndRenderpass(RenderpassType renderpassID);

	// ─────────────────────────────── Global State ────────────────────────────
	[[nodiscard]] bool UpdateGlobalWorldState(RenderpassType renderpassID,
											  const GlobalUBO& globalUBO);

	// ─────────────────────────────── Drawing ─────────────────────────────────
	[[nodiscard]] bool DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData);

	// ─────────────────────────────── Resources ───────────────────────────────
	[[nodiscard]] bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture);
	void DestroyTexture(ResourceTexture* texture);

	[[nodiscard]] bool CreateMaterial(ResourceMaterial* material);
	void DestroyMaterial(ResourceMaterial* material);

	[[nodiscard]] bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices,
									  uint32_t indexCount, const uint32_t* indices,
									  ResourceMesh* outGeometry);
	void DestroyGeometry(ResourceMesh* geometry);

	[[nodiscard]] bool CreateShader(ResourceShader* shader);
	void DestroyShader(ResourceShader* shader);
	bool ReloadShader(ResourceShader* shader) noexcept;
	bool ApplyCompiledShader(ResourceShader* shader) noexcept;

	// ─────────────────────────────── Picking ────────────────────────────────
	uint32_t PickObjectAt(int32_t pixelX, int32_t pixelY,
						  const glm::mat4& projection, const glm::mat4& view,
						  const std::vector<GeometryRenderData>& geometries);

	// ─────────────────────────────── Outlining ───────────────────────────────
	bool DrawOutlinedGeometries(RenderpassType renderpassID,
								const glm::mat4& projection, const glm::mat4& view,
								const std::vector<GeometryRenderData>& outlinedGeometries,
								const OutlineSettings& settings);

	// ─────────────────────────────── Editor Grid ─────────────────────────────
	bool DrawGrid(RenderpassType renderpassID,
	              const glm::mat4& projection, const glm::mat4& view);

	// ─────────────────────────────── Background ──────────────────────────────
	bool DrawBackground(RenderpassType renderpassID,
							   const glm::mat4& projection, const glm::mat4& view);

	// ─────────────────────────────── Bounding Boxes ──────────────────────────
	bool DrawBoundingBoxes(RenderpassType renderpassID,
	                       const glm::mat4& projection, const glm::mat4& view,
	                       const std::vector<BoundingBoxData>& boxes);

	// ─────────────────────────────── Camera Frustums ─────────────────────────
	bool DrawCameraFrustums(RenderpassType renderpassID,
	                        const glm::mat4& projection,
	                        const glm::mat4& view,
	                        const std::vector<CameraFrustumData>& frustums,
	                        bool globalAlreadySet = false);

	// ─────────────────────────────── Point Light Debugs ──────────────────────
	bool DrawPointLightDebugs(RenderpassType renderpassID,
	                          const glm::mat4& projection,
	                          const glm::mat4& view,
	                          const std::vector<BoundingBoxData>& lightDebugs,
	                          bool globalAlreadySet = false);

public:

	uint64_t mFrameNumber;

private:

	IRendererBackend* mBackendInterface;
	RendererBackendType mBackendType;

};

#endif // NOUS_ENGINE_RENDERER_BACKEND_H