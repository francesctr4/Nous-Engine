#include "ModuleEditor.h"
#include "ModuleInput.h"

#include "VulkanBackend.h"
#include "VulkanExternal.h"
#include "VulkanUtils.h"

ModuleEditor::ModuleEditor(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleEditor::~ModuleEditor()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleEditor::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForVulkan(GetSDLWindowData());
	ImGui_ImplVulkan_InitInfo imGuiVulkanInitInfo{};

	imGuiVulkanInitInfo.Allocator = vkContext->allocator;
	imGuiVulkanInitInfo.CheckVkResultFn = VK_CHECK_IMGUI;
	imGuiVulkanInitInfo.DescriptorPoolSize = 1;
	imGuiVulkanInitInfo.Device = vkContext->device.logicalDevice;
	imGuiVulkanInitInfo.ImageCount = vkContext->swapChain.swapChainImages.size();
	imGuiVulkanInitInfo.Instance = vkContext->instance;
	imGuiVulkanInitInfo.MSAASamples = vkContext->device.msaaSamples;
	imGuiVulkanInitInfo.PhysicalDevice = vkContext->device.physicalDevice;
	imGuiVulkanInitInfo.Queue = vkContext->device.graphicsQueue;
	imGuiVulkanInitInfo.QueueFamily = vkContext->device.graphicsQueueIndex;
	imGuiVulkanInitInfo.RenderPass = vkContext->mainRenderpass.handle;
	imGuiVulkanInitInfo.UseDynamicRendering = false;
	imGuiVulkanInitInfo.MinImageCount = 2;

	NOUS_ASSERT(ImGui_ImplVulkan_Init(&imGuiVulkanInitInfo));

	// (this gets a bit more complicated, see example app for full reference)
	//ImGui_ImplVulkan_CreateFontsTexture();

	return true;
}

bool ModuleEditor::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}

UpdateStatus ModuleEditor::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// (After event loop)
	// Start the Dear ImGui frame

	//ImGui_ImplVulkan_NewFrame();
	//ImGui_ImplSDL2_NewFrame();
	//ImGui::NewFrame();

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleEditor::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleEditor::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// Rendering
// (Your code clears your framebuffer, renders your other stuff etc.)

	/*ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), VulkanBackend::GetVulkanContext()->graphicsCommandBuffers[0].handle);*/
	// (Your code calls vkCmdEndRenderPass, vkQueueSubmit, vkQueuePresentKHR etc.)

	return UPDATE_CONTINUE;
}

bool ModuleEditor::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	return true;
}

void ModuleEditor::ReceiveEvent(const Event& event)
{

}

void ModuleEditor::ProcessImGuiEvent(const SDL_Event* event)
{
	ImGui_ImplSDL2_ProcessEvent(event);
}

void ModuleEditor::ImGuiNewFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void ModuleEditor::InitFrame()
{

}

void ModuleEditor::DrawEditor()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GetVulkanContext()->graphicsCommandBuffers[GetVulkanContext()->imageIndex].handle);
}

VulkanContext* ModuleEditor::GetVulkanContext()
{
	return VulkanBackend::GetVulkanContext();
}

void ModuleEditor::EndFrame(VkCommandBuffer commandBuffer)
{

}
