#ifndef RENDERERBACKEND_H
#define RENDERERBACKEND_H

#include "Renderer/RendererTypes.h"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

class RendererBackend
{
public:

	RendererBackend();
	virtual ~RendererBackend();

	bool Create(RendererBackendType bType);
	void Destroy();

	bool Initalize();
	void Shutdown();

	void Resized(uint16 width, uint16 height);

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	bool BeginRenderpass(BuiltInRenderpass renderpassID);
	bool EndRenderpass(BuiltInRenderpass renderpassID);

	bool UpdateGlobalWorldState(BuiltInRenderpass renderpassID, glm::mat4x4 projection, glm::mat4x4 view, glm::vec3 viewPosition, glm::vec4 ambientColor, int32 mode);
	bool UpdateGlobalUIState(BuiltInRenderpass renderpassID, glm::mat4x4 projection, glm::mat4x4 view, int32 mode);

	bool DrawGeometry(BuiltInRenderpass renderpassID, GeometryRenderData renderData);

	bool CreateTexture(const uint8* pixels, ResourceTexture* outTexture);
	bool DestroyTexture(ResourceTexture* texture);

	bool CreateMaterial(ResourceMaterial* material);
	bool DestroyMaterial(ResourceMaterial* material);

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry);
	bool DestroyGeometry(ResourceMesh* geometry);

	// -------------------------------------- \\

	uint64 frameNumber;

private:
	
	IRendererBackend* backendInterface;

};

#endif // RENDERERBACKEND_H