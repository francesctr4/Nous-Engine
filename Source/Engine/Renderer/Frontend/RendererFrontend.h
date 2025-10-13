#ifndef NOUS_ENGINE_RENDERER_FRONTEND_H
#define NOUS_ENGINE_RENDERER_FRONTEND_H

#include "Engine/Renderer/RendererTypes.h"
#include <functional>

// Forward declarations
class RendererBackend;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

/**
 * @brief High-level rendering controller.
 *
 * Orchestrates frame rendering, render passes, and delegates GPU operations to RendererBackend.
 */
class RendererFrontend
{
public:
	RendererFrontend();
	~RendererFrontend();

	// ---------------------------------------------------------------------
	// Accessors
	// ---------------------------------------------------------------------
	void SetBackendType(RendererBackendType backendType) noexcept { mBackendType = backendType; }
	[[nodiscard]] RendererBackendType GetBackendType() const noexcept { return mBackendType; }

	// ---------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------
	[[nodiscard]] bool Initialize(RendererBackendType backendType);
	void Shutdown();
	void OnResized(uint16_t width, uint16_t height);

	// ---------------------------------------------------------------------
	// Rendering
	// ---------------------------------------------------------------------
	[[nodiscard]] FrameResult DrawFrame(RenderPacket* packet);

	// ---------------------------------------------------------------------
	// GPU Resource Management
	// ---------------------------------------------------------------------
	[[nodiscard]] bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture);
	void DestroyTexture(ResourceTexture* texture);

	[[nodiscard]] bool CreateMaterial(ResourceMaterial* material);
	void DestroyMaterial(ResourceMaterial* material);

	[[nodiscard]] bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices,
									  uint32_t indexCount, const uint32_t* indices,
									  ResourceMesh* outGeometry);
	void DestroyGeometry(ResourceMesh* geometry);

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

};

#endif // NOUS_ENGINE_RENDERER_FRONTEND_H