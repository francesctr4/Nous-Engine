#include <Scripting/EngineAPI/Bindings/InputBindings.h>

#include <Scripting/iScriptInput.h>

static IScriptInput* s_input = nullptr;

void SetupInputBindings(InputAPI& input, IScriptInput* scriptInput)
{
    s_input = scriptInput;

    // All script-facing input is gated by IScriptInput::IsScriptInputEnabled() so the
    // editor can pause script input while the GameViewport isn't the focused panel
    // (otherwise camera scripts keep running while you click around the rest of the UI).
    // Capture state is intentionally NOT gated — scripts may legitimately query/release
    // capture during the gated frame, and the implementation already drops/restores capture
    // around the gate transitions.

    // Key state checking
    input.GetKey = [](NOUS_SCANCODE scancode) -> int {
        if (!s_input || !s_input->IsScriptInputEnabled()) return 0;
        return static_cast<int>(s_input->GetKey(static_cast<int>(scancode)));
    };

    // Mouse button checking
    input.GetMouseButton = [](int button) -> int {
        if (!s_input || !s_input->IsScriptInputEnabled()) return 0;
        return static_cast<int>(s_input->GetMouseButton(button));
    };

    // Mouse position
    input.GetMousePosition = [](int* x, int* y) {
        if (!s_input || !s_input->IsScriptInputEnabled()) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = s_input->GetMouseX();
        if (y) *y = s_input->GetMouseY();
    };

    // Mouse motion
    input.GetMouseMotion = [](int* x, int* y) {
        if (!s_input || !s_input->IsScriptInputEnabled()) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = s_input->GetMouseXMotion();
        if (y) *y = s_input->GetMouseYMotion();
    };

    // Mouse capture (relative mouse mode)
    input.SetMouseCaptured = [](bool captured) {
        if (!s_input) return;
        s_input->SetMouseCaptured(captured);
    };

    input.IsMouseCaptured = []() -> bool {
        if (!s_input) return false;
        return s_input->IsMouseCaptured();
    };
}
