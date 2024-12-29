#include "ModuleEditor.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"
#include "ModuleCamera3D.h"

#include "VulkanBackend.h"
#include "VulkanExternal.h"
#include "VulkanUtils.h"
#include "VulkanImGuiResources.h"

#include "IEditorWindow.inl"
#include "ImGuiCustom.h"

#pragma region EDITOR WINDOWS

#include "MainMenuBar.h"
#include "Properties.h"
#include "Assets.h"
#include "AssetsBrowser.h"

#pragma endregion

ModuleEditor::ModuleEditor(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	currentBackendType = RendererBackendType::UNKNOWN;

	AddEditorWindow(std::make_unique<MainMenuBar>("MainMenuBar"));
	AddEditorWindow(std::make_unique<Properties>("Properties"));
	AddEditorWindow(std::make_unique<Assets>("Assets"));
	AddEditorWindow(std::make_unique<AssetsBrowser>("AssetsBrowser"));
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

	ImGuiCustom::ImGuiTheme_RedGrey();

	io.Fonts->AddFontFromFileTTF("Assets/Fonts/tahoma.ttf", 15.0f);

	switch (currentBackendType)
	{
		case RendererBackendType::VULKAN:
		{
			VulkanContext* vkContext = GetVulkanContext();

			NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(vkContext);

			// Setup Platform/Renderer backends
			ImGui_ImplSDL2_InitForVulkan(GetSDLWindowData());

			ImGui_ImplVulkan_InitInfo imGuiVulkanInitInfo{};

			imGuiVulkanInitInfo.Allocator = vkContext->allocator;
			imGuiVulkanInitInfo.CheckVkResultFn = VK_CHECK_IMGUI;
			imGuiVulkanInitInfo.DescriptorPool = vkContext->imGuiResources.descriptorPool;
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
			NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(GetVulkanContext());
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
	// Set DockSpace Invisible Window Flags
	ImGuiWindowFlags window = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	// Get Window Viewport
	ImGuiViewport* viewport = ImGui::GetWindowViewport();

	// Set Window Parameters
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.0f);

	// Set Window Style Parameters
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	// Begin DockSpace Invisible Window with the flags
	ImGui::Begin("Dockspace", 0, window);

	// Apply Window Style Parameters
	ImGui::PopStyleVar(3);

	// Create DockSpace on the invisible window
	ImGui::DockSpace(ImGui::GetID("Dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// End DockSpace Window
	ImGui::End();

	// In your rendering loop
	if (ImGui::Begin("Main Menu")) 
	{
		// Add a button to toggle the window's state
		if (ImGui::Button("Toggle Properties")) 
		{
			ToggleWindowState(editorWindows, "Properties");
		}

		if (ImGui::Button("Toggle Assets"))
		{
			ToggleWindowState(editorWindows, "Assets");
		}
	}

	ImGui::End();

	ImGui::ShowDemoWindow();

	for (auto& window : editorWindows) 
	{
		window->Draw();
	}

	ImGui::Begin("Viewport");

	// Get the viewport panel size
	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

	// Texture dimensions (replace with your actual texture dimensions)
	float textureWidth = 1920.0f; // Example width
	float textureHeight = 1080.0f; // Example height

	// Calculate aspect ratios
	float textureAspect = textureWidth / textureHeight;
	float viewportAspect = viewportPanelSize.x / viewportPanelSize.y;

	// UV coordinates for cropping
	ImVec2 uvMin(0.0f, 0.0f); // Top-left corner of the texture
	ImVec2 uvMax(1.0f, 1.0f); // Bottom-right corner of the texture

	if (viewportAspect < textureAspect) {
		// Viewport is narrower: crop left and right
		float cropFactor = textureAspect / viewportAspect;
		uvMin.x = 0.5f - 0.5f / cropFactor;
		uvMax.x = 0.5f + 0.5f / cropFactor;
	}
	else if (viewportAspect > textureAspect) {
		// Viewport is wider: crop top and bottom
		float cropFactor = viewportAspect / textureAspect;
		uvMin.y = 0.5f - 0.5f / cropFactor;
		uvMax.y = 0.5f + 0.5f / cropFactor;
	}

	// Render the image with cropped UVs
	//ImGui::Image(
	//	(ImTextureID)ImGui_ImplVulkan_AddTexture(
	//		sampler,
	//		GetVulkanContext()->swapChain.swapChainImageViews[GetVulkanContext()->imageIndex],
	//		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	//	),
	//	ImVec2{ viewportPanelSize.x, viewportPanelSize.y },
	//	uvMin,
	//	uvMax
	//);

	ImGui::End();
}

void ModuleEditor::EndFrame(RendererBackendType backendType)
{
	ImGui::Render();

	switch (backendType)
	{
		case RendererBackendType::VULKAN:
		{
			//NOUS_ImGuiVulkanResources::RenderImGuiDrawData(GetVulkanContext(),
			//	&GetVulkanContext()->graphicsCommandBuffers[GetVulkanContext()->imageIndex], 
			//	&GetVulkanContext()->imGuiResources.viewportRenderPass);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GetVulkanContext()->graphicsCommandBuffers[GetVulkanContext()->imageIndex].handle);

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