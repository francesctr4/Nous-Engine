#pragma once

struct SceneAPI
{
    void (*LoadScene)  (const char* path) = nullptr;
    void (*ReloadScene)()                 = nullptr;
};

class IScriptSceneHost;
void SetupSceneBindings(SceneAPI& scene, IScriptSceneHost* sceneHost);
