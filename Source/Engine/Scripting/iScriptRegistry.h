#pragma once

#include <string>

class CScript;
class IScript;

// -----------------------------------------------------------------------------
// Script-component registry, seen from inside Systems/.
// -----------------------------------------------------------------------------
/**
 * @brief Registration surface for CScript components.
 *
 * Implemented by ScriptManager so CScript can register itself without reaching
 * through ModuleScene::scriptManager (which is a public raw member today).
 */
class IScriptRegistry
{
public:
    virtual ~IScriptRegistry() = default;

    virtual void RegisterScriptComponent(CScript* component)   = 0;
    virtual void UnregisterScriptComponent(CScript* component) = 0;

    // Instantiates a script type by name from the loaded script library.
    // Returns null when the name is not in the registry (or no library is loaded).
    // Ownership passes to the caller, which releases it via IScript::Destroy().
    virtual IScript* CreateScriptInstance(const std::string& scriptName) = 0;
};
