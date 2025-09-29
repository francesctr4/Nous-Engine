#ifndef NOUS_ENGINE_SCRIPTREGISTRY_H
#define NOUS_ENGINE_SCRIPTREGISTRY_H

#include <unordered_map>
#include <string>
#include <functional>
#include "ScriptRegistryExport.h"

class IScript;

class ScriptRegistry {
public:
    using Factory = std::function<IScript*()>;

    void Register(const std::string& name, Factory factory) {
        factories[name] = factory;
    }

    IScript* Create(const std::string& name) const {
        auto it = factories.find(name);
        return (it != factories.end()) ? it->second() : nullptr;
    }

    const std::unordered_map<std::string, Factory>& GetAll() const {
        return factories;
    }

private:
    std::unordered_map<std::string, Factory> factories;
};

// instead of exposing a global variable, expose a function:
extern "C" SCRIPTS_API ScriptRegistry* GetScriptRegistry();

#endif //NOUS_ENGINE_SCRIPTREGISTRY_H
