#include "InputBindings.h"

#include "Core/Application.h"
#include "Core/Modules/ModuleInput.h"

void SetupInputBindings(InputAPI &input)
{
    // Key state checking
    input.GetKey = [](int scancode) -> int {
        if (!External || !External->input) {
            return 0; // Return IDLE if input system not available
        }
        // Cast the engine's KeyState to int for scripting
        return static_cast<int>(External->input->GetKey(scancode));
    };

    // Mouse button checking
    input.GetMouseButton = [](int button) -> int {
        if (!External || !External->input) {
            return 0;
        }
        return static_cast<int>(External->input->GetMouseButton(button));
    };

    // Mouse position
    input.GetMousePosition = [](int* x, int* y) {
        if (!External || !External->input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->input->GetMouseX();
        if (y) *y = External->input->GetMouseY();
    };

    // Mouse motion
    input.GetMouseMotion = [](int* x, int* y) {
        if (!External || !External->input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->input->GetMouseXMotion();
        if (y) *y = External->input->GetMouseYMotion();
    };
}
