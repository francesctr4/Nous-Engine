#ifndef NOUS_ENGINE_COMPONENTSCRIPT_H
#define NOUS_ENGINE_COMPONENTSCRIPT_H

#include "Scripting System/Internal/IScript.inl"
#include "ECS/Component.h"

class CScript : public Component {
public:
    COMPONENT_TYPE(CScript)

    IScript* instance = nullptr;

//    template<typename T, typename... Args>
//    void Bind(Args&&... args) {
//        instance = new T(std::forward<Args>(args)...);
//        instance->Awake();
//    }
//
//    void Destroy() {
//        if (instance) {
//            instance->OnDestroy();
//            delete instance;
//            instance = nullptr;
//        }
//    }

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override
    {
        return nullptr;
    }

    void Deserialize(JSON_Object* obj) override
    {

    }
};

#endif //NOUS_ENGINE_COMPONENTSCRIPT_H
