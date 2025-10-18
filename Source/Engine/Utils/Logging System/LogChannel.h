#ifndef NOUS_ENGINE_LOGCHANNEL_H
#define NOUS_ENGINE_LOGCHANNEL_H

#include <cstdint>
#include <string_view>
#include <array>

// ------------------------------------------------------------
// Master channel list (define each only once here)
// ------------------------------------------------------------
#define LOG_CHANNEL_LIST \
    X(DEFAULT, "Default") \
    X(NOUS_ENGINE_RENDERER_BACKEND_VULKAN_BACKEND, "VulkanBackend.cpp") \
    X(NOUSENGINE_SYSTEMS_MEMORYMANAGER, "MemoryManager.cpp") \
    X(NOUSENGINE_SYSTEMS_EVENTSYSTEM, "EventSystem.cpp") \
    X(NOUSEDITOR_CORE_MODULEEDITOR, "ModuleEditor.cpp") \

// ------------------------------------------------------------
// Enum generation
// ------------------------------------------------------------
enum class LogChannel : uint16_t {
#define X(NAME, STRING) NAME,
    LOG_CHANNEL_LIST
#undef X
    MAX_CHANNELS
};

// ------------------------------------------------------------
// String table generation
// ------------------------------------------------------------
constexpr std::array<const char*, static_cast<size_t>(LogChannel::MAX_CHANNELS)>
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
    size_t idx = static_cast<size_t>(channel);
    if (idx < LOG_CHANNEL_NAMES.size())
        return LOG_CHANNEL_NAMES[idx];
    return "Unknown";
}

#endif // NOUS_ENGINE_LOGCHANNEL_H
