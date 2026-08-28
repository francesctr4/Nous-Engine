#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

/// @brief The SystemManager coordinates initialization, shutdown, and access
/// to all engine systems (ResourceManager, ECS, CameraSystem, etc.).
/// Currently a dummy implementation to keep systems.lib valid.
class SystemManager
{
public:

    SystemManager();
    ~SystemManager();

    /// @brief Initialize all registered systems.
    void Initialize();

    /// @brief Shutdown and clean up systems.
    void Shutdown();

    /// @brief Dummy helper to show extension points.
    void EnableSystem(const std::string& name);
    void DisableSystem(const std::string& name);

private:

    // Track system enable/disable states
    std::unordered_map<std::string, bool> m_SystemStates;

};

