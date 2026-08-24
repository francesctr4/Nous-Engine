#pragma once

class ISceneHost;
class IAudioBroker;
class IVideoBroker;
class IResourceLoader;
class IScriptRegistry;

// -----------------------------------------------------------------------------
// The engine services a component may reach, as interfaces owned by Systems/.
// -----------------------------------------------------------------------------
/**
 * @brief The seam that keeps Systems/ from depending on Modules/.
 *
 * Application assembles one of these after its module graph is built and hands it
 * to ModuleScene, which passes it to every Scene it creates; Scene publishes it
 * through entt's registry context, and Component::Services() resolves it.
 *
 * EVERY FIELD IS NULLABLE AND NULL IS A SUPPORTED STATE, NOT AN ERROR. A headless
 * Scene (every unit-test fixture) wires nothing, and components must no-op rather
 * than crash. Guard the individual service you use, never the aggregate —
 * Services() itself never returns null.
 */
struct ComponentServices
{
    ISceneHost*      host      = nullptr;
    IAudioBroker*    audio     = nullptr;
    IVideoBroker*    video     = nullptr;
    IResourceLoader* resources = nullptr;
    IScriptRegistry* scripts   = nullptr;
};
