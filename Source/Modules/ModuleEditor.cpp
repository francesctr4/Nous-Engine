#include "Modules/ModuleEditor.h"

#include "Modules/ModuleRenderer3D.h"
#include "Renderer/RendererFrontend.h"
#include "Modules/ModuleCamera3D.h"

#include "Renderer/Vulkan/VulkanBackend.h"
#include "Renderer/Vulkan/VulkanExternal.h"
#include "Renderer/Vulkan/VulkanUtils.h"
#include "Renderer/Vulkan/VulkanImGuiResources.h"

#include "Editor/IEditorWindow.inl"
#include "Editor/ImGuiConfig/ImGuiCustom.h"
#include "Includes/ImGui.h"

#pragma region EDITOR WINDOWS

#include "Editor/Windows/MainMenuBar.h"
#include "Editor/Windows/AssetsBrowser.h"
#include "Editor/Windows/ResourcesWindow.h"
#include "Editor/Windows/MultithreadingWindow.h"
#include "Editor/Windows/JobQueueWindow.h"
#include "Editor/Windows/SceneViewport.h"
#include "Editor/Windows/GameViewport.h"
#include "Editor/Windows/HierarchyWindow.h"
#include "Editor/Windows/InspectorWindow.h"
#include "Editor/Windows/ConsoleWindow.h"

#pragma endregion

ModuleEditor::ModuleEditor(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	currentBackendType = RendererBackendType::UNKNOWN;
}

ModuleEditor::~ModuleEditor()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

// Array to store ImFont pointers
std::vector<ImFont*> ModuleEditor::fonts;

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

	fonts.push_back(io.Fonts->AddFontFromFileTTF(R"(Assets\Fonts\tahoma.ttf)", 15.0f));

	// Optionally, load more fonts as needed
	fonts.push_back(io.Fonts->AddFontFromFileTTF(R"(Assets\Fonts\tahoma.ttf)", 12.0f));

	switch (currentBackendType)
	{
		case RendererBackendType::VULKAN:
		{
			VulkanContext* vkContext = GetVulkanContext();

			// Setup Platform/Renderer backends
			ImGui_ImplSDL3_InitForVulkan(GetSDLWindowData());

			ImGui_ImplVulkan_InitInfo imGuiVulkanInitInfo{};

			imGuiVulkanInitInfo.Allocator = vkContext->allocator;
			imGuiVulkanInitInfo.CheckVkResultFn = VK_CHECK_IMGUI;
			imGuiVulkanInitInfo.DescriptorPool = vkContext->imGuiResources.descriptorPool;
			imGuiVulkanInitInfo.Device = vkContext->device.logicalDevice;
			imGuiVulkanInitInfo.ImageCount = vkContext->swapChain.swapChainImages.size();
			imGuiVulkanInitInfo.Instance = vkContext->instance;
			imGuiVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			imGuiVulkanInitInfo.PhysicalDevice = vkContext->device.physicalDevice;
			imGuiVulkanInitInfo.Queue = vkContext->device.graphicsQueue;
			imGuiVulkanInitInfo.QueueFamily = vkContext->device.graphicsQueueIndex;
			imGuiVulkanInitInfo.RenderPass = vkContext->uiRenderpass.handle;
			imGuiVulkanInitInfo.UseDynamicRendering = false;
			imGuiVulkanInitInfo.MinImageCount = 2;

			NOUS_ASSERT(ImGui_ImplVulkan_Init(&imGuiVulkanInitInfo))

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

	AddEditorWindow(std::make_unique<MainMenuBar>("MainMenuBar"));
	AddEditorWindow(std::make_unique<AssetsBrowser>("Assets"));
	AddEditorWindow(std::make_unique<Resources>("Resources"));
	AddEditorWindow(std::make_unique<Multithreading>("Multithreading"));
	AddEditorWindow(std::make_unique<JobQueue>("Job Queue"));
	AddEditorWindow(std::make_unique<GameViewport>("Game"));
	AddEditorWindow(std::make_unique<SceneViewport>("Scene"));
	AddEditorWindow(std::make_unique<HierarchyWindow>("Hierarchy"));
    AddEditorWindow(std::make_unique<InspectorWindow>("Inspector"));
    AddEditorWindow(std::make_unique<ConsoleWindow>("Console"));

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

	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	return true;
}

void ModuleEditor::ProcessInputEvent(const SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);
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

	ImGui_ImplSDL3_NewFrame();
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

	ImGui::ShowDemoWindow();

	//ImGui::Begin("camerapos");
	//ImGui::Text("%f,%f,%f", App->camera->GetCamera()->GetPos().x, App->camera->GetCamera()->GetPos().y, App->camera->GetCamera()->GetPos().z);
	//ImGui::End();

	for (auto& win : editorWindows)
	{
		win->Draw();
	}
}

void ModuleEditor::EndFrame(RendererBackendType backendType)
{
	ImGui::Render();

	switch (backendType)
	{
		case RendererBackendType::VULKAN:
		{
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

IEditorWindow* ModuleEditor::GetEditorWindowByName(std::string name)
{
	IEditorWindow* window = nullptr;

	for (auto& w : editorWindows) 
	{
		if (strcmp(w->GetTitle(), name.c_str()) == 0)
		{
			window = w.get();
		}
	}

	return window;
}