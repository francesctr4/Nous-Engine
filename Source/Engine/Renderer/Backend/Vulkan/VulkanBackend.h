#ifndef VULKANBACKEND_H
#define VULKANBACKEND_H

#include <Engine/Renderer/Backend/RendererBackend.h>
#include <Engine/Core/EngineExport.h>

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

// --------------- Vulkan Renderer Backend --------------- \\

struct VulkanContext;
struct VulkanCommandBuffer;

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	~VulkanBackend() override;

	bool Initialize() override;
	void Shutdown() noexcept override;

	void Resized(uint16 width, uint16 height) noexcept override;

	FrameResult BeginFrame(float dt) override;
	FrameResult EndFrame(float dt) override;

	bool BeginRenderpass(RenderpassType renderpassID) override;
	bool EndRenderpass(RenderpassType renderpassID) override;

	bool RecreateResources();

	bool UpdateGlobalWorldState(
            RenderpassType renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            const glm::vec3& viewPosition, const glm::vec4& ambientColor,
            int32 mode) override;

	bool UpdateGlobalUIState(
            RenderpassType renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            int32 mode) override;

	bool DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData) override;

	// ----------------------------------------------------------------------------------------------- //
	// TEMPORAL //

	bool CreateTexture(const uint8* pixels, ResourceTexture* outTexture) override;
	void DestroyTexture(ResourceTexture* texture) noexcept override;

	bool CreateMaterial(ResourceMaterial* material) override;
    void DestroyMaterial(ResourceMaterial* material) noexcept override;

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* geometry) override;
    void DestroyGeometry(ResourceMesh* geometry) noexcept override;

	NOUS_ENGINE_API static VulkanContext* GetVulkanContext();

	void ProcessPendingSubmissions();

	VulkanCommandBuffer* GetCommandBufferByRenderpassID(RenderpassType renderpassID);

private:

	static VulkanContext* vkContext;

};

#endif // VULKANBACKEND_H