#ifndef RENDERERFRONTEND_H
#define RENDERERFRONTEND_H

#include "Renderer/RendererTypes.h"
#include "Core/Globals.h"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

class RendererBackend;

class RendererFrontend
{
public:

	RendererFrontend();
	virtual ~RendererFrontend();

	bool Initialize(RendererBackendType backendType);
	void Shutdown();

	void OnResized(uint16 width, uint16 height);

	bool DrawFrame(RenderPacket* packet);

	void CreateTexture(const uint8* pixels, ResourceTexture* outTexture);
	void DestroyTexture(ResourceTexture* texture);

	bool CreateMaterial(ResourceMaterial* material);
	void DestroyMaterial(ResourceMaterial* material);

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry);
	void DestroyGeometry(ResourceMesh* geometry);

private:

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	bool BeginRenderpass(RenderpassType renderpassID);
	bool EndRenderpass(RenderpassType renderpassID);

	void UpdateGlobalWorldState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, glm::vec3 viewPosition, glm::vec4 ambientColor, int32 mode);
	void UpdateGlobalUIState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, int32 mode);

	void DrawGeometry(RenderpassType renderpassID, GeometryRenderData renderData);
	void DrawEditor();

public:

	RendererBackendType backendType;

private:

	RendererBackend* backend;

};

#endif // RENDERERFRONTEND_H