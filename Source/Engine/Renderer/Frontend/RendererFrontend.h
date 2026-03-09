#ifndef NOUS_ENGINE_RENDERER_FRONTEND_H
#define NOUS_ENGINE_RENDERER_FRONTEND_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/EngineExport.h"

#include <functional>

// Forward declarations
class RendererBackend;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;
struct IEditorOverlay;

/**
 * @brief High-level rendering controller.
 *
 * Orchestrates frame rendering, render passes, and delegates GPU operations to RendererBackend.
 */
class RendererFrontend
{
public:
    NOUS_ENGINE_API RendererFrontend();
	NOUS_ENGINE_API ~RendererFrontend();

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
	[[nodiscard]] NOUS_ENGINE_API bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture);
	NOUS_ENGINE_API void DestroyTexture(ResourceTexture* texture);

	[[nodiscard]] NOUS_ENGINE_API bool CreateMaterial(ResourceMaterial* material);
	NOUS_ENGINE_API void DestroyMaterial(ResourceMaterial* material);

	[[nodiscard]] NOUS_ENGINE_API bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices,
									  uint32_t indexCount, const uint32_t* indices,
									  ResourceMesh* outGeometry);
	NOUS_ENGINE_API void DestroyGeometry(ResourceMesh* geometry);

	[[nodiscard]] NOUS_ENGINE_API bool CreateShader(ResourceShader* shader);
	NOUS_ENGINE_API void DestroyShader(ResourceShader* shader);

	// ---------------------------------------------------------------------
	// Accessors
	// ---------------------------------------------------------------------
	NOUS_ENGINE_API void SetBackendType(RendererBackendType backendType) noexcept;
	[[nodiscard]] NOUS_ENGINE_API enum RendererBackendType GetBackendType() const noexcept;

	NOUS_ENGINE_API void SetEditorOverlay(IEditorOverlay* overlay);

private:
	// ---------------------------------------------------------------------
	// Internal helpers
	// ---------------------------------------------------------------------
	[[nodiscard]] FrameResult BeginFrame(float dt);
	[[nodiscard]] FrameResult EndFrame(float dt);

	[[nodiscard]] bool ExecuteRenderpass(RenderpassType pass, const std::function<void()>& drawCommands);

	void DrawEditor();

private:

	RendererBackend* mBackend;
	RendererBackendType mBackendType;

	IEditorOverlay* mEditorOverlay;

};

#endif // NOUS_ENGINE_RENDERER_FRONTEND_H