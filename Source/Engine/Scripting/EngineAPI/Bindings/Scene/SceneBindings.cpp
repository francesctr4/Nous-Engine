#include "Engine/Scripting/EngineAPI/Bindings/Scene/SceneBindings.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Core/Logger/Logger.h"

static ModuleScene* s_scene = nullptr;

void SetupSceneBindings(SceneAPI& scene, ModuleScene* moduleScene)
{
    s_scene = moduleScene;

    scene.LoadScene = [](const char* path) {
        if (!s_scene || !path) return;
        s_scene->LoadSceneAsync(path);
    };

    scene.ReloadScene = []() {
        if (!s_scene) return;
        if (!s_scene->HasCurrentScenePath()) {
            NOUS_WARN("[SceneAPI] ReloadScene called but no scene has been saved/loaded.");
            return;
        }
        s_scene->LoadSceneAsync(s_scene->GetCurrentScenePath());
    };
}
