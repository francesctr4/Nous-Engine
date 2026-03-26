#include <Engine/Scripting/EngineAPI/Bindings/Input/InputBindings.h>

#include <Engine/Core/Application.h>
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"

void SetupInputBindings(InputAPI &input)
{
    // Key state checking
    input.GetKey = [](int scancode) -> int {
        if (!External || !External->GetInput()) {
            return 0; // Return IDLE if input system not available
        }
        // Cast the engine's KeyState to int for scripting
        return static_cast<int>(External->GetInput()->GetKey(scancode));
    };

    // Mouse button checking
    input.GetMouseButton = [](int button) -> int {
        if (!External || !External->GetInput()) {
            return 0;
        }
        return static_cast<int>(External->GetInput()->GetMouseButton(button));
    };

    // Mouse position
    input.GetMousePosition = [](int* x, int* y) {
        if (!External || !External->GetInput()) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->GetInput()->GetMouseX();
        if (y) *y = External->GetInput()->GetMouseY();
    };

    // Mouse motion
    input.GetMouseMotion = [](int* x, int* y) {
        if (!External || !External->GetInput()) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->GetInput()->GetMouseXMotion();
        if (y) *y = External->GetInput()->GetMouseYMotion();
    };
}
