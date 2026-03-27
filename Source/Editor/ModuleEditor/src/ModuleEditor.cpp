#include "Editor/ModuleEditor/include/ModuleEditor.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/UI/ImGuiCustom/ImGuiCustom.h"

// ImGui_Temp
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include <ImGuizmo.h>

#pragma region EDITOR WINDOWS

#include "Editor/UI/Windows/MainMenuBar/include/MainMenuBar.h"
#include "Editor/UI/Windows/AssetsBrowser/include/AssetsBrowser.h"
#include "Editor/UI/Windows/ResourcesWindow/include/ResourcesWindow.h"
#include "Editor/UI/Windows/MultithreadingWindow/include/MultithreadingWindow.h"
#include "Editor/UI/Windows/JobQueueWindow/include/JobQueueWindow.h"
#include "Editor/UI/Windows/SceneViewport/include/SceneViewport.h"
#include "Editor/UI/Windows/GameViewport/include/GameViewport.h"
#include "Editor/UI/Windows/HierarchyWindow/include/HierarchyWindow.h"
#include "Editor/UI/Windows/InspectorWindow/include/InspectorWindow.h"
#include "Editor/UI/Windows/ConsoleWindow/include/ConsoleWindow.h"
#include "Editor/UI/Windows/MemoryWindow/include/MemoryWindow.h"

#pragma endregion

#include <vector>
#include <memory>

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_EDITOR_MODULE_EDITOR;

ModuleEditor::ModuleEditor(Application* app,
	ModuleWindow* moduleWindow,
	ModuleInput* moduleInput,
	ModuleCamera3D* moduleCamera3D,
	ModuleResourceManager* moduleResourceManager,
	ModuleScene* moduleScene,
	ModuleRenderer3D* moduleRenderer3D)
: Module(app), mModuleWindow(moduleWindow), mModuleInput(moduleInput), mModuleCamera3D(moduleCamera3D),
   mModuleResourceManager(moduleResourceManager), mModuleScene(moduleScene), mModuleRenderer3D(moduleRenderer3D),
  editorWindows(MemoryTag::EDITOR), fonts(MemoryTag::EDITOR)
{
	currentBackendType = RendererBackendType::UNKNOWN;

    eventSystem->Subscribe(EventType::INPUT_EVENT, this);
    eventSystem->Subscribe(EventType::IMGUI_RECREATION, this);
}

ModuleEditor::~ModuleEditor()
{

}

bool ModuleEditor::Awake()
{
	NOUS_FileManager::CopyFile(R"(Assets\Settings\imgui.ini)", "imgui.ini");

	mModuleRenderer3D->GetRendererFrontend()->SetEditorOverlay(this);
	currentBackendType = mModuleRenderer3D->GetRendererFrontend()->GetBackendType();

	// Setup Dear ImGui_Temp context
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
            NOUS_INFO_C(CURRENT_CHANNEL, "Using Renderer Backend: %d (Vulkan)", currentBackendType);
			VulkanContext* vkContext = GetVulkanContext();

			// Setup Platform/Renderer backends
			ImGui_ImplSDL3_InitForVulkan(mModuleWindow->GetSDL_Window());

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

	AddEditorWindow(NOUS_NEW<MainMenuBar>(MemoryTag::EDITOR, "MainMenuBar", this));
	AddEditorWindow(NOUS_NEW<AssetsBrowser>(MemoryTag::EDITOR, "Assets", this));
	AddEditorWindow(NOUS_NEW<Resources>(MemoryTag::EDITOR, "Resources", this));
	AddEditorWindow(NOUS_NEW<Multithreading>(MemoryTag::EDITOR, "Multithreading", this));
	AddEditorWindow(NOUS_NEW<JobQueue>(MemoryTag::EDITOR, "Job Queue", this));
	AddEditorWindow(NOUS_NEW<GameViewport>(MemoryTag::EDITOR, "Game", this));
	AddEditorWindow(NOUS_NEW<SceneViewport>(MemoryTag::EDITOR, "Scene", this));
	AddEditorWindow(NOUS_NEW<HierarchyWindow>(MemoryTag::EDITOR, "Hierarchy", this));
	AddEditorWindow(NOUS_NEW<InspectorWindow>(MemoryTag::EDITOR, "Inspector", this));
	AddEditorWindow(NOUS_NEW<ConsoleWindow>(MemoryTag::EDITOR, "Console", this));
	AddEditorWindow(NOUS_NEW<MemoryWindow>(MemoryTag::EDITOR, "Memory Manager", this));

	return true;
}

bool ModuleEditor::Start()
{
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
	// Wait for the GPU and free command buffers/framebuffers BEFORE destroying
	// any Vulkan resources.  The last frame's command buffers still reference
	// ImGui's vertex/index buffers, pipeline, and descriptor sets, and the
	// framebuffers still reference the viewport image views.
	mModuleRenderer3D->GetRendererFrontend()->ReleaseFrameResources();

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

    for (auto* win : editorWindows)
    {
        NOUS_DELETE(win, MemoryTag::EDITOR);
    }
	editorWindows.clear();

	return true;
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
	ImGuizmo::BeginFrame();
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

void ModuleEditor::AddEditorWindow(IEditorWindow* editorWindow)
{
	editorWindows.push_back(editorWindow);
}

IEditorWindow* ModuleEditor::GetEditorWindowByName(std::string name)
{
	for (auto* w : editorWindows)
	{
		if (strcmp(w->GetTitle(), name.c_str()) == 0)
			return w;
	}
	return nullptr;
}

void ModuleEditor::OnEvent(const Event &event)
{
	switch (event.type)
	{
		case EventType::INPUT_EVENT:
		{
			auto* sdlEvent = reinterpret_cast<SDL_Event*>(event.ctx.u64[0]);
            ImGui_ImplSDL3_ProcessEvent(sdlEvent);
			break;
		}
		case EventType::IMGUI_RECREATION:
		{
            switch (currentBackendType)
            {
                case RendererBackendType::VULKAN:
                {
                    VulkanContext* vkContext = GetVulkanContext();

                    GameViewport::DestroyGameViewportDescriptorSets();
                    SceneViewport::DestroySceneViewportDescriptorSets();

                    NOUS_ImGuiVulkanResources::RecreateImGuiVulkanResources(vkContext);

                    SceneViewport::CreateSceneViewportDescriptorSets();
                    GameViewport::CreateGameViewportDescriptorSets();
                    break;
                }
                default: break;
            }
			break;
		}
		default: break;
	}
}

RendererFrontend* ModuleEditor::GetRendererFrontend() const
{
    return mModuleRenderer3D->GetRendererFrontend();
}

ImFont *ModuleEditor::GetFont(size_t index) const
{
    if (index >= fonts.size())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "ModuleEditor::GetFont called with out-of-range index: %zu", index);
        return nullptr;
    }

    return fonts[index];
}
