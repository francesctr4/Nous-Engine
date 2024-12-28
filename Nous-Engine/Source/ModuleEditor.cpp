#include "ModuleEditor.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"

#include "VulkanBackend.h"
#include "VulkanExternal.h"
#include "VulkanUtils.h"

#include "IEditorWindow.inl"

#pragma region EDITOR WINDOWS

#include "Properties.h"
#include "Assets.h"

#pragma endregion

ModuleEditor::ModuleEditor(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	currentBackendType = RendererBackendType::UNKNOWN;

	AddEditorWindow(std::make_unique<Properties>());
	AddEditorWindow(std::make_unique<Assets>());
}

ModuleEditor::~ModuleEditor()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleEditor::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	currentBackendType = ModuleRenderer3D::rendererFrontend->backendType;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	switch (currentBackendType)
	{
		case RendererBackendType::VULKAN:
		{
			VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

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

			break;
		}
		case RendererBackendType::OPENGL:
		{

			break;
		}
		case RendererBackendType::DIRECTX:
		{

			break;
		}
		default:
		{

			break;
		}
	}

	return true;
}

void ModuleEditor::DrawEditor()
{
	InitFrame(currentBackendType);

	InternalDrawEditor();

	EndFrame(currentBackendType);
}

bool ModuleEditor::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	switch (currentBackendType)
	{
		case RendererBackendType::VULKAN:
		{
			ImGui_ImplVulkan_Shutdown();
			break;
		}

		case RendererBackendType::OPENGL:
		{

			break;
		}
		case RendererBackendType::DIRECTX:
		{

			break;
		}
		default:
		{

			break;
		}
	}

	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	return true;
}

void ModuleEditor::ProcessInputEvent(const SDL_Event* event)
{
	ImGui_ImplSDL2_ProcessEvent(event);
}

void ModuleEditor::InitFrame(RendererBackendType backendType)
{
	switch (backendType)
	{
		case RendererBackendType::VULKAN: 
		{
			ImGui_ImplVulkan_NewFrame();
			break;
		}
		
		case RendererBackendType::OPENGL:
		{

			break;
		}
		case RendererBackendType::DIRECTX:
		{

			break;
		}
		default:
		{

			break;
		}
	}

	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void ModuleEditor::InternalDrawEditor()
{
	ImGui::ShowDemoWindow();

	for (auto& w : editorWindows) 
	{
		w->Draw();
	}
}

void ModuleEditor::EndFrame(RendererBackendType backendType)
{
	ImGui::Render();

	switch (backendType)
	{
		case RendererBackendType::VULKAN:
		{
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), 
				GetVulkanContext()->graphicsCommandBuffers[GetVulkanContext()->imageIndex].handle);

			break;
		}

		case RendererBackendType::OPENGL:
		{

			break;
		}
		case RendererBackendType::DIRECTX:
		{

			break;
		}
		default:
		{

			break;
		}
	}
}

VulkanContext* ModuleEditor::GetVulkanContext()
{
	return VulkanBackend::GetVulkanContext();
}

void ModuleEditor::AddEditorWindow(std::unique_ptr<IEditorWindow> editorWindow)
{
	editorWindows.emplace_back(std::move(editorWindow));
}