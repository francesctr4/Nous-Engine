#include "Editor/ModuleEditor/include/ModuleEditor.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Core/EventSystem/EventSystem.h"

#include "Engine/Renderer/iEditorRenderBridge.h"

#include "Editor/UI/IEditorWindow.h"
#include "Editor/UI/ImGuiCustom/ImGuiCustom.h"

// ImGui_Temp
#include "imgui.h"
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
#include "Editor/UI/Windows/InspectorWindow/InspectorWindow.h"
#include "Editor/UI/Windows/ConsoleWindow/include/ConsoleWindow.h"
#include "Editor/UI/Windows/MemoryWindow/include/MemoryWindow.h"
#include "Editor/UI/Windows/TextEditorWindow/include/TextEditorWindow.h"
#include "Editor/UI/Windows/AudioGraphEditor/include/AudioGraphEditor.h"
#include "Editor/UI/Windows/AudioMixer/include/AudioMixerWindow.h"

#pragma endregion

#include <vector>
#include <memory>
#include <SDL3/SDL_keyboard.h>

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/Logger/Logger.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_EDITOR_MODULE_EDITOR;

ModuleEditor::ModuleEditor(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
	ModuleWindow* moduleWindow,
	ModuleInput* moduleInput,
	ModuleCamera3D* moduleCamera3D,
	ModuleResourceManager* moduleResourceManager,
	ModuleScene* moduleScene,
	ModuleRenderer3D* moduleRenderer3D)
: Module(eventSystem, jobSystem), mModuleWindow(moduleWindow), mModuleInput(moduleInput), mModuleCamera3D(moduleCamera3D),
   mModuleResourceManager(moduleResourceManager), mModuleScene(moduleScene), mModuleRenderer3D(moduleRenderer3D),
  editorWindows(MemoryTag::EDITOR), fonts(MemoryTag::EDITOR)
{
	currentBackendType = RendererBackendType::UNKNOWN;

    eventSystem->Subscribe(EventType::INPUT_EVENT, this);
    eventSystem->Subscribe(EventType::IMGUI_RECREATION, this);
    eventSystem->Subscribe(EventType::DROP_FILE, this);
}

ModuleEditor::~ModuleEditor() = default;

bool ModuleEditor::Awake()
{
	nous::engine::filesystem::CopyFile("Assets/Settings/imgui.ini", "imgui.ini");

	mModuleRenderer3D->GetRendererFrontend()->SetEditorOverlay(this);
	currentBackendType = mModuleRenderer3D->GetRendererFrontend()->GetBackendType();

	// Resolve here, not in the ctor: MainEditor.cpp constructs this module before
	// App->Awake(), and the backend that implements the bridge is created inside
	// ModuleRenderer3D::Awake().
	m_renderBridge = mModuleRenderer3D->GetRendererFrontend()->GetEditorBridge();

	// Setup Dear ImGui_Temp context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	nous::editor::imgui::ImGuiTheme_RedGrey();

	fonts.push_back(io.Fonts->AddFontFromFileTTF("Assets/Fonts/tahoma.ttf", 15.0f));

	// Optionally, load more fonts as needed
	fonts.push_back(io.Fonts->AddFontFromFileTTF("Assets/Fonts/tahoma.ttf", 12.0f));

	// Font Awesome icon font (fonts[2]) — used by AssetsBrowser for file-type icons
	{
		static constexpr ImWchar c_iconRanges[] = { 0xE000, 0xF8FF, 0 };
		ImFontConfig iconConfig;
		iconConfig.MergeMode = false;
		iconConfig.PixelSnapH = true;
		iconConfig.GlyphMinAdvanceX = 64.0f;
		fonts.push_back(io.Fonts->AddFontFromFileTTF("Assets/Fonts/fa-solid-900.ttf", 64.0f, &iconConfig, c_iconRanges));
	}

	switch (currentBackendType)
	{
		case RendererBackendType::VULKAN:
		{
            NOUS_INFO_C(CURRENT_CHANNEL, "Using Renderer Backend: %d (Vulkan)", currentBackendType);

			NOUS_ASSERT(m_renderBridge)
			const EditorGpuInfo gpu = m_renderBridge->GetGpuInfo();

			// Setup Platform/Renderer backends
			ImGui_ImplSDL3_InitForVulkan(mModuleWindow->GetSDL_Window());

			ImGui_ImplVulkan_InitInfo imGuiVulkanInitInfo{};

			imGuiVulkanInitInfo.Allocator = gpu.allocator;
			imGuiVulkanInitInfo.CheckVkResultFn = gpu.checkVkResultFn;
			imGuiVulkanInitInfo.DescriptorPool = gpu.descriptorPool;
			imGuiVulkanInitInfo.Device = gpu.device;
			imGuiVulkanInitInfo.ImageCount = gpu.imageCount;
			imGuiVulkanInitInfo.Instance = gpu.instance;
			imGuiVulkanInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			imGuiVulkanInitInfo.PhysicalDevice = gpu.physicalDevice;
			imGuiVulkanInitInfo.Queue = gpu.graphicsQueue;
			imGuiVulkanInitInfo.QueueFamily = gpu.graphicsQueueFamily;
			imGuiVulkanInitInfo.PipelineInfoMain.RenderPass = gpu.uiRenderpass;
			imGuiVulkanInitInfo.UseDynamicRendering = false;
			imGuiVulkanInitInfo.MinImageCount = 2;

			NOUS_ASSERT(ImGui_ImplVulkan_Init(&imGuiVulkanInitInfo))
			break;
		}
		// case RendererBackendType::OPENGL:
		// {
		//
		// 	break;
		// }
		// case RendererBackendType::DIRECTX:
		// {
		//
		// 	break;
		// }
		default:
		{

			break;
		}
	}

    m_gameExporter = NOUS_NEW<GameExporter>(MemoryTag::EDITOR);

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
	AddEditorWindow(NOUS_NEW<TextEditorWindow>(MemoryTag::EDITOR, "Text Editor", this));
	AddEditorWindow(NOUS_NEW<AudioGraphEditor>(MemoryTag::EDITOR, "Audio Graph Editor", this));
	AddEditorWindow(NOUS_NEW<AudioMixerWindow>(MemoryTag::EDITOR, "Audio Mixer", this));

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

	// The ImGui SDL3 backend (v1.91+) only calls SDL_StartTextInput/SDL_StopTextInput
	// through its IME callback, which only fires for built-in InputText widgets.
	// Custom text widgets (e.g. ImGuiColorTextEdit) set io.WantTextInput but get no
	// SDL text input, so typed characters never arrive. Bridge the gap here.
	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput)
		SDL_StartTextInput(mModuleWindow->GetSDL_Window());
	else
		SDL_StopTextInput(mModuleWindow->GetSDL_Window());

	mModuleInput->SetImGuiCaptureKeyboard(io.WantTextInput);
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
			if (m_renderBridge)
				m_renderBridge->DestroyImGuiResources();

			break;
		}
		// case RendererBackendType::OPENGL:
		// {
		//
		// 	break;
		// }
		// case RendererBackendType::DIRECTX:
		// {
		//
		// 	break;
		// }
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

    NOUS_DELETE(m_gameExporter, MemoryTag::EDITOR);
    m_gameExporter = nullptr;

	return true;
}

void ModuleEditor::InitFrame(const RendererBackendType backendType)
{
	switch (backendType)
	{
		case RendererBackendType::VULKAN:
		{
			ImGui_ImplVulkan_NewFrame();
			break;
		}
		// case RendererBackendType::OPENGL:
		// {
		//
		// 	break;
		// }
		// case RendererBackendType::DIRECTX:
		// {
		//
		// 	break;
		// }
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
	constexpr ImGuiWindowFlags window = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	// Get Window Viewport
	const ImGuiViewport* viewport = ImGui::GetWindowViewport();

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
	ImGui::Begin("Dockspace", nullptr, window);

	// Apply Window Style Parameters
	ImGui::PopStyleVar(3);

	// Create DockSpace on the invisible window
	ImGui::DockSpace(ImGui::GetID("Dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// End DockSpace Window
	ImGui::End();

	ImGui::ShowDemoWindow();

	// Default the script-input gate to disabled each frame; GameViewport::Begin() will
	// flip it back on when (and only when) its window has focus this frame. Doing the
	// reset here covers the closed-window case too: if GameViewport isn't being drawn,
	// nothing re-enables the gate and scripts stay paused.
	if (mModuleInput)
		mModuleInput->SetScriptInputEnabled(false);

	for (const auto& win : editorWindows)
	{
		win->Draw();
	}
}

void ModuleEditor::EndFrame(const RendererBackendType backendType) const
{
	ImGui::Render();

	switch (backendType)
	{
		case RendererBackendType::VULKAN:
		{
			if (m_renderBridge)
				ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_renderBridge->GetCurrentUICommandBuffer());

			break;
		}
		// case RendererBackendType::OPENGL:
		// {
		//
		// 	break;
		// }
		// case RendererBackendType::DIRECTX:
		// {
		//
		// 	break;
		// }
		default:
		{

			break;
		}
	}
}

void ModuleEditor::AddEditorWindow(IEditorWindow* editorWindow)
{
	editorWindows.push_back(editorWindow);
}

void ModuleEditor::UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath)
{
    mModuleRenderer3D->UpdateShaderWatcherPath(oldPath, newPath);
}

void ModuleEditor::WatchShaderFile(const std::string& path)
{
    mModuleRenderer3D->WatchShaderFile(path);
}

std::string ModuleEditor::GetAssetsBrowserDirectory() const
{
    if (IEditorWindow* w = const_cast<ModuleEditor*>(this)->GetEditorWindowByName("Assets"))
        return static_cast<AssetsBrowser*>(w)->current_directory;
    return "Assets";
}

IEditorWindow* ModuleEditor::GetEditorWindowByName(const std::string& name)
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
			const SDL_Event* sdlEvent = static_cast<SDL_Event*>(event.ctx.ptr[0]);
			ImGui_ImplSDL3_ProcessEvent(sdlEvent);
			break;
		}
		case EventType::DROP_FILE:
		{
			const std::string path = event.ctx.c ? event.ctx.c : "";
			if (!path.empty())
			{
				for (auto* w : editorWindows)
					w->OnFileDrop(path);
			}
			break;
		}
		case EventType::IMGUI_RECREATION:
		{
            switch (currentBackendType)
            {
                case RendererBackendType::VULKAN:
                {
                    if (!m_renderBridge)
                        break;

                    // The image views change, so the descriptor sets must be torn
                    // down before the recreate and rebuilt from the new images after.
                    GameViewport::DestroyGameViewportDescriptorSets(m_renderBridge);
                    SceneViewport::DestroySceneViewportDescriptorSets(m_renderBridge);

                    m_renderBridge->RecreateImGuiResources();

                    SceneViewport::CreateSceneViewportDescriptorSets(m_renderBridge);
                    GameViewport::CreateGameViewportDescriptorSets(m_renderBridge);
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

ImFont *ModuleEditor::GetFont(const size_t index) const
{
    if (index >= fonts.size())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "ModuleEditor::GetFont called with out-of-range index: %zu", index);
        return nullptr;
    }

    return fonts[index];
}
