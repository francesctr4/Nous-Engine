#ifndef NOUS_ENGINE_SCRIPTREGISTRATION_H
#define NOUS_ENGINE_SCRIPTREGISTRATION_H

#include "ScriptRegistry.h"

#define REGISTER_SCRIPT(CLASS) \
namespace { \
    struct AutoRegister_##CLASS { \
        AutoRegister_##CLASS() { \
            g_ScriptRegistry.Register(#CLASS, [](){ return new CLASS(); }); \
        } \
    }; \
    AutoRegister_##CLASS autoRegisterInstance_##CLASS; \
}

#endif //NOUS_ENGINE_SCRIPTREGISTRATION_H
