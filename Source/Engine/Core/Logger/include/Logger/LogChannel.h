#ifndef NOUS_ENGINE_LOGCHANNEL_H
#define NOUS_ENGINE_LOGCHANNEL_H

#include <cstdint>
#include <array>
#include <utility>
#include <cstddef>

// ------------------------------------------------------------------------------------------------
// LOG CHANNEL LIST
// ------------------------------------------------------------------------------------------------

#define LOG_CHANNEL_LIST \
    X(DEFAULT, "Default")\
    \
    /* ============================ EDITOR ============================ */ \
    X(NOUS_EDITOR_MAIN, "MainEditor.cpp") \
    X(NOUS_EDITOR_MODULE_EDITOR, "ModuleEditor.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_MAINMENUBAR, "MainMenuBar.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_ASSETSBROWSER, "AssetsBrowser.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_RESOURCESWINDOW, "ResourcesWindow.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_MULTITHREADINGWINDOW, "MultithreadingWindow.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_JOBQUEUEWINDOW, "JobQueueWindow.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_SCENEVIEWPORT, "SceneViewport.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_GAMEVIEWPORT, "GameViewport.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_HIERARCHYWINDOW, "HierarchyWindow.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_INSPECTORWINDOW, "InspectorWindow.cpp") \
    X(NOUS_EDITOR_UI_WINDOW_CONSOLEWINDOW, "ConsoleWindow.cpp") \
    \
    /* ============================ CORE ============================ */   \
    X(NOUS_ENGINE_MAIN, "MainEngine.cpp") \
    X(NOUS_ENGINE_CORE_APPLICATION, "NOUS_ENGINE_CORE_APPLICATION") \
    X(NOUS_ENGINE_MODULE_RENDERER3D, "MODULE_RENDERER3D") \
    X(NOUS_ENGINE_MODULE_AUDIO, "MODULE_AUDIO") \
    X(NOUS_ENGINE_MODULE_VIDEO, "MODULE_VIDEO") \
    X(NOUS_ENGINE_MODULE_UI, "MODULE_UI") \
    X(NOUS_ENGINE_MODULE_AI, "MODULE_AI") \
    X(NOUS_ENGINE_MODULE_ANIMATION, "MODULE_ANIMATION") \
    X(NOUS_ENGINE_MODULE_PHYSICS, "MODULE_PHYSICS") \
    X(NOUS_ENGINE_MODULE_PARTICLES, "MODULE_PARTICLES") \
    X(NOUS_ENGINE_CORE_MODULE_CAMERA3D, "ModuleCamera3D.cpp") \
    X(NOUS_ENGINE_CORE_MODULE_WINDOW, "ModuleWindow.cpp") \
    X(NOUS_ENGINE_CORE_MODULE_SCENE, "ModuleScene.cpp") \
    X(NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER, "RESOURCE_MANAGER") \
    X(NOUS_ENGINE_CORE_MODULE_INPUT, "ModuleInput.cpp") \
    \
    /* ============================ ECS ============================ */ \
    \
    /* ============================ MULTITHREADING ============================ */ \
    X(NOUS_ENGINE_MULTITHREADING, "NOUS_Multithreading") \
    \
    /* ============================ RENDERER FRONTEND ============================ */ \
    X(NOUS_ENGINE_RENDERER_FRONTEND, "RendererFrontend.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND, "RendererBackend.cpp") \
    \
    /* ============================ RENDERER BACKEND: VULKAN ============================ */ \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_BACKEND, "VulkanBackend.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_INSTANCE, "VulkanInstance.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_DEVICE, "VulkanDevice.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_SWAPCHAIN, "VulkanSwapchain.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_RENDERPASS, "VulkanRenderpass.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_FRAMEBUFFER, "VulkanFramebuffer.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_SYNCOBJECTS, "VulkanSyncObjects.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_PIPELINE, "VulkanGraphicsPipeline.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER, "VulkanCommandBuffer.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_IMAGE, "VulkanImage.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_BUFFER, "VulkanBuffer.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_DEBUGMESSENGER, "VulkanDebugMessenger.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_UTILS, "VulkanUtils.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_IMGUI_RESOURCES, "VulkanImGuiResources.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_UI_SHADER, "VulkanUIShader.cpp") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_MATERIAL_SHADER, "VulkanMaterialShader.cpp") \
    \
    /* ============================ SCRIPTING ============================ */ \
    X(NOUS_ENGINE_SCRIPTING_ENGINEAPI, "EngineAPI.cpp") \
    X(NOUS_ENGINE_SCRIPTING_SCRIPTBINDINGS, "ScriptBindings.cpp") \
    X(NOUS_ENGINE_SCRIPTING_LOGGERBINDINGS, "LoggerBindings.cpp") \
    X(NOUS_ENGINE_SCRIPTING_INPUTBINDINGS, "InputBindings.cpp") \
    X(NOUS_ENGINE_SCRIPTING_GAMEOBJECTBINDINGS, "GameObjectBindings.cpp") \
    X(NOUS_ENGINE_SCRIPTING_SCRIPT_MANAGER, "ScriptManager.cpp") \
    X(NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, "SCRIPT") \
    \
    /* ============================ ENGINE SYSTEMS ============================ */ \
    X(NOUS_ENGINE_SYSTEM_EVENTSYSTEM, "EventSystem.cpp") \
    X(NOUS_ENGINE_SYSTEM_MEMORYMANAGER, "MemoryManager.cpp") \
    X(NOUS_ENGINE_SYSTEM_TIMEMANAGER, "TimeManager.cpp") \
    X(NOUS_ENGINE_SYSTEM_FILESYSTEM, "FileManager.cpp") \
    X(NOUS_ENGINE_SYSTEM_RESOURCEMANAGER, "ResourceManager.cpp") \
    X(NOUS_ENGINE_SYSTEM_SHADERSYSTEM, "SHADER_SYSTEM") \
    X(NOUS_ENGINE_SYSTEM_AUDIOSYSTEM, "AUDIO_SYSTEM") \
    X(NOUS_ENGINE_SYSTEM_VIDEOSYSTEM, "VIDEO_SYSTEM") \
    \
    /* ============================ UTILS ============================ */ \
    X(NOUS_ENGINE_UTILS_LOGGER, "Logger.cpp") \
    X(NOUS_ENGINE_UTILS_RANDOM, "Random.cpp") \
    X(NOUS_ENGINE_UTILS_JSONFILE, "JsonFile.cpp") \
    X(NOUS_ENGINE_UTILS_ASSERTS, "Asserts.h") \
    \
    /* ============================ FEATURES ============================ */ \
    X(NOUS_ENGINE_EDITOR_FEATURE_MOUSE_PICKING, "MOUSE_PICKING")

// ------------------------------------------------------------
// Enum generation
// ------------------------------------------------------------
enum class LogChannel : uint8_t
{
#define X(NAME, STRING) NAME,
    LOG_CHANNEL_LIST
#undef X
    MAX_CHANNELS
};

// ------------------------------------------------------------
// String table generation
// ------------------------------------------------------------
constexpr std::array<const char*, static_cast<size_t>(std::to_underlying(LogChannel::MAX_CHANNELS))>
LOG_CHANNEL_NAMES = {
#define X(NAME, STRING) STRING,
    LOG_CHANNEL_LIST
#undef X
};

// ------------------------------------------------------------
// Helper
// ------------------------------------------------------------
constexpr const char* GetChannelName(LogChannel channel)
{
    if (const auto idx = static_cast<size_t>(std::to_underlying(channel)); idx < LOG_CHANNEL_NAMES.size())
        return LOG_CHANNEL_NAMES[idx];
    return "Unknown";
}

#endif // NOUS_ENGINE_LOGCHANNEL_H
