#pragma once

class Camera;

// -----------------------------------------------------------------------------
// The scene's host module, seen from inside Systems/.
// -----------------------------------------------------------------------------
/**
 * @brief Simulation state and viewport info, as needed by components.
 *
 * Implemented by ModuleScene so that components in Systems/ can read simulation
 * state and viewport info without depending on Modules/. Mirrors the existing
 * IResourceLoader seam (ScenePreloader -> ModuleResourceManager).
 *
 * Sim-state and viewport are deliberately one interface: CCamera is the only
 * consumer of the viewport half and it needs the sim-state half too, so
 * splitting would produce a single-client interface for no gain.
 */
class ISceneHost
{
public:
    virtual ~ISceneHost() = default;

    // ─────────────────────────────── Simulation state ────────────────────────
    virtual bool IsPlaying()      const = 0;
    virtual bool IsPaused()       const = 0;
    virtual bool IsStopped()      const = 0;
    virtual bool IsLoadingScene() const = 0;

    // ─────────────────────────────── Viewport ────────────────────────────────

    /** @brief Aspect ratio of the window the scene renders into. */
    virtual float GetWindowAspect() const = 0;

    /** @brief The camera that CCamera's main-camera instance drives. May be null. */
    virtual Camera* GetGameCamera() const = 0;
};
