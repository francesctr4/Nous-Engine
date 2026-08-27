#pragma once

#include <string>

class Scene;

// -----------------------------------------------------------------------------
// The scene's host module, as seen from inside Scripting/.
// -----------------------------------------------------------------------------
/**
 * @brief The active scene plus scene-loading control, as needed by the bindings.
 *
 * Implemented by ModuleScene so that Scripting/ can serve the script-facing
 * GameObject / Component / Light / Material / Camera / Scene APIs without
 * depending on Modules/.
 *
 * Deliberately NOT the ISceneHost in Systems/ECS: that one is sized to what
 * components need (simulation state and viewport info), and the two client sets
 * do not overlap at all. Same module, two consumers, two interfaces.
 *
 * GetActiveScene() replaces direct reads of ModuleScene's public `activeScene`
 * member -- a raw member cannot cross an interface, and it was the single most
 * common thing the bindings reached through the module for.
 */
class IScriptSceneHost
{
public:
    virtual ~IScriptSceneHost() = default;

    /**
     * @brief The scene scripts operate on. NULL between a scene teardown and the
     *        next load, so every binding must null-check it.
     */
    [[nodiscard]] virtual Scene* GetActiveScene() const = 0;

    /**
     * @brief Queues a scene swap. Returns immediately -- the load runs on a job
     *        and the swap lands on a later frame, so the caller must not assume
     *        GetActiveScene() has changed when this returns.
     */
    virtual void LoadSceneAsync(const std::string& path) = 0;

    /** @brief Path of the scene last saved or loaded; empty if there is none. */
    [[nodiscard]] virtual const std::string& GetCurrentScenePath() const = 0;

    /** @brief False for a scene that has never been saved to disk, in which case
     *         GetCurrentScenePath() has nothing to reload. */
    [[nodiscard]] virtual bool HasCurrentScenePath() const = 0;
};
