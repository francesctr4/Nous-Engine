#include "Engine/Core/Globals.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <parson.h>
#include <string>
#include <filesystem>

enum class GameState
{
    Creation,
    Start,
    Update,
    Finish,
    Exit,
};

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MAIN;

struct GameConfig
{
    std::string startScene = "Library/Scenes/LagiacrusScene.nous";
    float       targetFPS  = DEFAULT_TARGET_FPS;
};

static GameConfig LoadGameConfig(const char* argv0)
{
    GameConfig cfg;

    const std::string configPath =
        (std::filesystem::path(argv0).parent_path() / "Library" / "Settings" / "game_config.json").string();

    JSON_Value* root = json_parse_file(configPath.c_str());
    if (!root)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "game_config.json not found at '%s' — using defaults.", configPath.c_str());
        return cfg;
    }

    const JSON_Object* obj = json_value_get_object(root);

    if (const char* scene = json_object_get_string(obj, "startScene"))
        cfg.startScene = scene;

    const double fps = json_object_get_number(obj, "targetFPS");
    if (fps > 0.0)
        cfg.targetFPS = static_cast<float>(fps);

    json_value_free(root);

    NOUS_INFO_C(CURRENT_CHANNEL, "Loaded game_config.json: scene='%s', targetFPS=%.0f",
        cfg.startScene.c_str(), cfg.targetFPS);

    return cfg;
}

int main(int argc, char** argv)
{
    StartLogTimer();

    MemoryManager::InitializeMemory(MiB(300));

    NOUS_Multithreading::RegisterMainThread();

    InitializeLogging(false); // no console.log in game builds

    NOUS_INFO_C(CURRENT_CHANNEL, "Starting GameApp '%s'...", TITLE);

    int mainReturn = EXIT_FAILURE;
    GameState state = GameState::Creation;
    Application* App = nullptr;
    GameConfig cfg;

    // Guards PressPlay until the async scene load job finishes.
    bool sceneReady = false;

    while (state != GameState::Exit)
    {
        switch (state)
        {
            case GameState::Creation:
            {
                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Creation ----------");

                App = NOUS_NEW<Application>(MemoryTag::APPLICATION);

                // Configure GAME mode before Awake() so every module can branch on it.
                App->GetRenderer()->SetRenderMode(RenderMode::GAME);

                cfg = LoadGameConfig(argv[0]);
                App->SetTargetFPS(cfg.targetFPS);

                state = GameState::Start;
                break;
            }

            case GameState::Start:
            {
                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Awake ----------");

                if (!App->Awake())
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "Application Awake exits with ERROR");
                    state = GameState::Exit;
                    break;
                }

                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Start ----------");

                if (!App->Start())
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "Application Start exits with ERROR");
                    state = GameState::Exit;
                    break;
                }

                // Fullscreen after Start so the window/swapchain are fully initialized.
                App->GetWindow()->SetFullscreen(true);

                // Kick off async scene load. PressPlay is deferred until the job finishes.
                App->GetScene()->LoadSceneAsync(cfg.startScene);

                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Update ----------");
                state = GameState::Update;
                break;
            }

            case GameState::Update:
            {
                // Once the async scene load job completes, start simulation.
                if (!sceneReady && App->GetJobSystem()->GetPendingJobs() == 0)
                {
                    App->GetScene()->PressPlay();
                    sceneReady = true;
                    NOUS_INFO_C(CURRENT_CHANNEL, "Scene loaded — simulation started.");
                }

                const UpdateStatus updateReturn = App->Update();

                if (updateReturn == UpdateStatus::ERROR)
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "Application Update exits with ERROR");
                    state = GameState::Exit;
                }
                else if (updateReturn == UpdateStatus::STOP)
                {
                    state = GameState::Finish;
                }
                break;
            }

            case GameState::Finish:
            {
                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application CleanUp ----------");

                if (!App->CleanUp())
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "Application CleanUp exits with ERROR");
                }
                else
                {
                    mainReturn = EXIT_SUCCESS;
                }

                state = GameState::Exit;
                break;
            }

            default: break;
        }
    }

    // All jobs were drained in CleanUp(), so no in-flight lambda can race on this.
    External = nullptr;

    NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Destruction ----------");
    NOUS_DELETE(App, MemoryTag::APPLICATION);

    NOUS_INFO_C(CURRENT_CHANNEL, "GameApp exited successfully.");

    NOUS_Multithreading::UnregisterMainThread();

    NOUS_MULTILINE_C(LOG_LEVEL_INFO, CURRENT_CHANNEL,
        (std::string("[main] ") + MemoryManager::GetMemoryUsageStats()).c_str());

    ShutdownLogging();

    MemoryManager::ShutdownMemory();

    return mainReturn;
}