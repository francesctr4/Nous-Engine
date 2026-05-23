#pragma once

struct SceneAPI
{
    void (*LoadScene)  (const char* path) = nullptr;
    void (*ReloadScene)()                 = nullptr;
};

class ModuleScene;
void SetupSceneBindings(SceneAPI& scene, ModuleScene* moduleScene);
