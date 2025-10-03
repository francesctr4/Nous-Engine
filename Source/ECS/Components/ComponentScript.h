#ifndef NOUS_ENGINE_COMPONENTSCRIPT_H
#define NOUS_ENGINE_COMPONENTSCRIPT_H

#include "Scripting System/Internal/IScript.inl"

struct CScript {
    IScript* instance = nullptr;

    template<typename T, typename... Args>
    void Bind(Args&&... args) {
        instance = new T(std::forward<Args>(args)...);
        instance->Awake();
    }

    void Destroy() {
        if (instance) {
            instance->OnDestroy();
            delete instance;
            instance = nullptr;
        }
    }
};

#endif //NOUS_ENGINE_COMPONENTSCRIPT_H
