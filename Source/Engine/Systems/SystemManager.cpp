#include "SystemManager.h"

#include "Engine/Core/Logger/Logger.h"

SystemManager::SystemManager()
{

}

SystemManager::~SystemManager()
{
    Shutdown();
}

void SystemManager::Initialize()
{
    NOUS_INFO("[SystemManager] Initializing systems...");

    NOUS_INFO("[SystemManager] Systems initialized successfully.");
}

void SystemManager::Shutdown()
{
    NOUS_INFO("[SystemManager] Shutting down systems...");

    NOUS_INFO("[SystemManager] Systems shut down successfully.");
}

void SystemManager::EnableSystem(const std::string& name)
{
    m_SystemStates[name] = true;
    NOUS_INFO("[SystemManager] Enabled system: %s", name.c_str());
}

void SystemManager::DisableSystem(const std::string& name)
{
    m_SystemStates[name] = false;
    NOUS_INFO("[SystemManager] Disabled system: %s", name.c_str());
}