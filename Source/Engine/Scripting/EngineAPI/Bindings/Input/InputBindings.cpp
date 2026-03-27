#include <Engine/Scripting/EngineAPI/Bindings/Input/InputBindings.h>

#include "Engine/Modules/ModuleInput/include/ModuleInput.h"

static ModuleInput* s_input = nullptr;

void SetupInputBindings(InputAPI& input, ModuleInput* moduleInput)
{
    s_input = moduleInput;

    // Key state checking
    input.GetKey = [](int scancode) -> int {
        if (!s_input) return 0;
        return static_cast<int>(s_input->GetKey(scancode));
    };

    // Mouse button checking
    input.GetMouseButton = [](int button) -> int {
        if (!s_input) return 0;
        return static_cast<int>(s_input->GetMouseButton(button));
    };

    // Mouse position
    input.GetMousePosition = [](int* x, int* y) {
        if (!s_input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = s_input->GetMouseX();
        if (y) *y = s_input->GetMouseY();
    };

    // Mouse motion
    input.GetMouseMotion = [](int* x, int* y) {
        if (!s_input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = s_input->GetMouseXMotion();
        if (y) *y = s_input->GetMouseYMotion();
    };
}
