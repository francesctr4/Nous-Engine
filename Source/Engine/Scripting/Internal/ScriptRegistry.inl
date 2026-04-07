#ifndef NOUS_ENGINE_SCRIPTREGISTRY_INL
#define NOUS_ENGINE_SCRIPTREGISTRY_INL

#include <unordered_map>
#include <string>
#include <functional>
#include <cstdio>
#include <utility>

// Export macros
#if defined(_WIN32) || defined(_WIN64)
#ifdef SCRIPTS_EXPORTS
#define SCRIPTS_API __declspec(dllexport)
#else
#define SCRIPTS_API __declspec(dllimport)
#endif
#else
#define SCRIPTS_API __attribute__((visibility("default")))
#endif

class IScript;

class ScriptRegistry {
public:
    using Factory = std::function<IScript*()>;

    void Register(const std::string& name, Factory factory) {
        factories[name] = std::move(factory);
        printf("[ScriptRegistry] Registered: %s", name.c_str());
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

// Global registry accessor
extern "C" SCRIPTS_API inline ScriptRegistry* GetScriptRegistry()
{
    static ScriptRegistry instance;
    return &instance;
}

// Automatic registration macro
#define REGISTER_SCRIPT(CLASS) \
namespace { \
    struct AutoRegister_##CLASS { \
        AutoRegister_##CLASS() { \
            GetScriptRegistry()->Register(#CLASS, [](){ return new CLASS(); }); \
        } \
    }; \
    static AutoRegister_##CLASS autoRegisterInstance_##CLASS; \
}

#endif //NOUS_ENGINE_SCRIPTREGISTRY_INL
