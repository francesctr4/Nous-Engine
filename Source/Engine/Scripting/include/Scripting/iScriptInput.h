#pragma once

#include <EngineCore/Globals.h>
#include <EngineCore/IInputReader.h>

// -----------------------------------------------------------------------------
// Input, as seen from inside Scripting/.
// -----------------------------------------------------------------------------
/**
 * @brief Everything the script-facing InputAPI bindings poll.
 *
 * Implemented by ModuleInput so that Scripting/ can serve the script input API
 * without depending on Modules/. Mirrors the IResourceLoader / ISceneHost /
 * IRenderWindow seams elsewhere in the engine.
 *
 * Extends IInputReader (Core/) rather than restating its four methods, and
 * rather than widening IInputReader itself: that interface's other client,
 * ModuleCamera3D, has no business with a capture mutator or with the editor's
 * script-input gate. Interfaces are sized to their client set -- these two
 * clients genuinely differ, so they get two interfaces, related by inheritance
 * where they overlap.
 *
 * Inherited from IInputReader: GetKey, GetMouseButton, GetMouseXMotion,
 * GetMouseYMotion, GetMouseZ.
 */
class IScriptInput : public IInputReader
{
public:
    ~IScriptInput() override = default;

    // ─────────────────────────────── Mouse position ──────────────────────────
    // Absolute window coordinates, as opposed to IInputReader's per-frame deltas.
    [[nodiscard]] virtual int32 GetMouseX() const = 0;
    [[nodiscard]] virtual int32 GetMouseY() const = 0;

    // ─────────────────────────────── Mouse capture ───────────────────────────
    // Relative mouse mode, for first-person camera scripts.
    virtual void SetMouseCaptured(bool captured) = 0;
    [[nodiscard]] virtual bool IsMouseCaptured() const = 0;

    // ─────────────────────────────── Editor gate ─────────────────────────────
    /**
     * @brief False while the editor is focused somewhere other than the
     *        GameViewport, so script input goes quiet instead of letting camera
     *        scripts keep running while you click around the rest of the UI.
     *
     * Always true in a standalone game. The bindings gate every read on this;
     * capture state is deliberately NOT gated (see SetupInputBindings).
     */
    [[nodiscard]] virtual bool IsScriptInputEnabled() const = 0;
};
