#pragma once

class CScript;

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
};
