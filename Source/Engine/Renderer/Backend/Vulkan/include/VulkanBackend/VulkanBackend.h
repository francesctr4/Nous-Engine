#ifndef VULKANBACKEND_H
#define VULKANBACKEND_H

#include <RendererBackend/iRendererBackend.h>
#include <Renderer/iEditorRenderBridge.h>
#include <EngineCore/EngineExport.h>
#include <cstdint>

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;

// --------------- Vulkan Renderer Backend --------------- \\

struct VulkanContext;
struct VulkanCommandBuffer;

class VulkanBackend : public IRendererBackend, public IEditorRenderBridge
{
public:

	VulkanBackend();
	~VulkanBackend() override;

	void InjectDependencies(
		EventSystem* eventSystem,
		nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		IRenderWindow* window,
		IRenderResourceProvider* resourceProvider) override;

	bool Initialize() override;
	void Shutdown() noexcept override;
	void SetRenderMode(RenderMode mode) noexcept override;
	void ReleaseFrameResources() noexcept override;

	void Resized(uint16_t width, uint16_t height) noexcept override;

	FrameResult BeginFrame(float dt) override;
	FrameResult EndFrame(float dt) override;

	bool BeginRenderpass(RenderpassType renderpassID) override;
	bool EndRenderpass(RenderpassType renderpassID) override;

	bool RecreateResources();

	bool UpdateGlobalWorldState(
            RenderpassType renderpassID,
            const GlobalUBO& globalUBO) override;

	bool DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData) override;

    void UploadInstanceData(const glm::mat4* matrices,
                            const uint32_t*  paletteBases,
                            uint32_t         count,
                            uint32_t         instanceOffset,
                            const glm::mat4* palettes,
                            uint32_t         boneCount,
                            uint32_t         paletteOffset) override;

    bool DrawGeometryBatched(RenderpassType renderpassID,
                             const InstancedBatch& batch) override;

	// ----------------------------------------------------------------------------------------------- //
	// TEMPORAL //

	bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture) override;
	void DestroyTexture(ResourceTexture* texture) noexcept override;

	bool UpdateDynamicTexture(const uint8_t* pixels, ResourceTexture* texture) override;

	bool CreateMaterial(ResourceMaterial* material) override;
    void DestroyMaterial(ResourceMaterial* material) noexcept override;

	bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices, uint32_t indexCount, const uint32_t* indices, ResourceMesh* geometry) override;
    void DestroyGeometry(ResourceMesh* geometry) noexcept override;

	bool CreateShader(ResourceShader* shader) override;
	void DestroyShader(ResourceShader* shader) noexcept override;
	bool ReloadShader(ResourceShader* shader) noexcept override;
	bool ApplyCompiledShader(ResourceShader* shader) noexcept override;
	void WaitForGPUIdle() noexcept override;

	uint32_t PickObjectAt(int32_t pixelX, int32_t pixelY,
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

	bool DrawWireframeMeshInstances(RenderpassType renderpassID,
	                                const glm::mat4& projection,
	                                const glm::mat4& view,
	                                WireframeMesh mesh,
	                                const std::vector<WireframeInstance>& instances) override;

	bool DrawCameraFrustums(RenderpassType renderpassID,
	                        const glm::mat4& projection,
	                        const glm::mat4& view,
	                        const std::vector<CameraFrustumData>& frustums) override;

	IEditorRenderBridge* GetEditorBridge() noexcept override { return this; }

	// ─────────────────────────── IEditorRenderBridge ─────────────────────────

	[[nodiscard]] EditorGpuInfo GetGpuInfo() const override;
	void DestroyImGuiResources() override;
	void RecreateImGuiResources() override;
	[[nodiscard]] VkCommandBuffer GetCurrentUICommandBuffer() const override;

	[[nodiscard]] uint64_t GetViewportTexture(EditorViewport viewport) const override;
	[[nodiscard]] EditorViewportImages GetViewportImages(EditorViewport viewport) const override;
	void SetViewportDescriptorSets(EditorViewport viewport,
	                               std::vector<VkDescriptorSet> descriptorSets) override;
	[[nodiscard]] std::vector<VkDescriptorSet> TakeViewportDescriptorSets(EditorViewport viewport) override;

	void GetFramebufferSize(int32_t* outWidth, int32_t* outHeight) const override;
	void RecreateWorkerCommandPools() override;

private:

	void ProcessPendingSubmissions();
	VulkanCommandBuffer* GetCommandBufferByRenderpassID(RenderpassType renderpassID);

	// Per-instance, not static: a static here was the ambient global the editor
	// reached through. The editor now gets the narrow IEditorRenderBridge instead.
	VulkanContext* vkContext = nullptr;
	bool m_frameResourcesReleased = false;

	int32_t m_cachedFramebufferWidth  = 0;
	int32_t m_cachedFramebufferHeight = 0;

};

#endif // VULKANBACKEND_H