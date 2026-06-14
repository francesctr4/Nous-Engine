#ifndef VULKANBACKEND_H
#define VULKANBACKEND_H

#include "Engine/Renderer/Backend/iRendererBackend.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/Globals.h"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;

// --------------- Vulkan Renderer Backend --------------- \\

struct VulkanContext;
struct VulkanCommandBuffer;

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	~VulkanBackend() override;

	void InjectDependencies(
		EventSystem* eventSystem,
		nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		ModuleWindow* window,
		ModuleResourceManager* resourceManager) override;

	bool Initialize() override;
	void Shutdown() noexcept override;
	void SetRenderMode(RenderMode mode) noexcept override;
	void ReleaseFrameResources() noexcept override;

	void Resized(uint16 width, uint16 height) noexcept override;

	FrameResult BeginFrame(float dt) override;
	FrameResult EndFrame(float dt) override;

	bool BeginRenderpass(RenderpassType renderpassID) override;
	bool EndRenderpass(RenderpassType renderpassID) override;

	bool RecreateResources();

	bool UpdateGlobalWorldState(
            RenderpassType renderpassID,
            const GlobalUBO& globalUBO) override;

	bool DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData) override;

    void UploadInstanceMatrices(const glm::mat4* matrices,
                                uint32_t count,
                                uint32_t instanceOffset) override;

    bool DrawGeometryBatched(RenderpassType renderpassID,
                             const InstancedBatch& batch) override;

	// ----------------------------------------------------------------------------------------------- //
	// TEMPORAL //

	bool CreateTexture(const uint8* pixels, ResourceTexture* outTexture) override;
	void DestroyTexture(ResourceTexture* texture) noexcept override;

	bool UpdateDynamicTexture(const uint8* pixels, ResourceTexture* texture) override;

	bool CreateMaterial(ResourceMaterial* material) override;
    void DestroyMaterial(ResourceMaterial* material) noexcept override;

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* geometry) override;
    void DestroyGeometry(ResourceMesh* geometry) noexcept override;

	bool CreateShader(ResourceShader* shader) override;
	void DestroyShader(ResourceShader* shader) noexcept override;
	bool ReloadShader(ResourceShader* shader) noexcept override;
	bool ApplyCompiledShader(ResourceShader* shader) noexcept override;
	void WaitForGPUIdle() noexcept override;

	uint32 PickObjectAt(int32 pixelX, int32 pixelY,
						const glm::mat4& projection, const glm::mat4& view,
						const std::vector<GeometryRenderData>& geometries) override;

	bool DrawOutlinedGeometries(RenderpassType renderpassID,
								const glm::mat4& projection, const glm::mat4& view,
								const std::vector<GeometryRenderData>& outlinedGeometries,
								const OutlineSettings& settings) override;

	bool DrawGrid(RenderpassType renderpassID,
	              const glm::mat4& projection, const glm::mat4& view) override;

	bool DrawBackground(RenderpassType renderpassID,
	                    const glm::mat4& projection,
	                    const glm::mat4& view) override;

	bool DrawBoundingBoxes(RenderpassType renderpassID,
	                       const glm::mat4& projection,
	                       const glm::mat4& view,
	                       const std::vector<BoundingBoxData>& boxes) override;

	bool DrawCameraFrustums(RenderpassType renderpassID,
	                        const glm::mat4& projection,
	                        const glm::mat4& view,
	                        const std::vector<CameraFrustumData>& frustums,
	                        bool globalAlreadySet = false) override;

	bool DrawPointLightDebugs(RenderpassType renderpassID,
	                          const glm::mat4& projection,
	                          const glm::mat4& view,
	                          const std::vector<BoundingBoxData>& lightDebugs,
	                          bool globalAlreadySet = false) override;

	bool DrawDirectionalLightDebugs(RenderpassType renderpassID,
	                                const glm::mat4& projection,
	                                const glm::mat4& view,
	                                const std::vector<DirectionalLightDebugData>& lightDebugs,
	                                bool globalAlreadySet = false) override;

	bool DrawSpotLightDebugs(RenderpassType renderpassID,
	                         const glm::mat4& projection,
	                         const glm::mat4& view,
	                         const std::vector<SpotLightDebugData>& lightDebugs,
	                         bool globalAlreadySet = false) override;

	NOUS_ENGINE_API static VulkanContext* GetVulkanContext();

	static void ProcessPendingSubmissions();
	static VulkanCommandBuffer* GetCommandBufferByRenderpassID(RenderpassType renderpassID);

private:

	static VulkanContext* vkContext;
	bool m_frameResourcesReleased = false;

	int32 m_cachedFramebufferWidth  = 0;
	int32 m_cachedFramebufferHeight = 0;

};

#endif // VULKANBACKEND_H