#include <Core/Application.h>
#include <CrashHandler/CrashHandler.h>
#include <EngineCore/AppConfig.h>
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <NOUS_Multithreading/NOUS_Multithreading.h>
#include <ModuleWindow/ModuleWindow.h>
#include <ModuleScene/ModuleScene.h>

#include <Utils/Serialization/JsonFile.h>
#include <string>
#include <filesystem>

enum class GameState : uint8_t
{
    Creation,
    Start,
    Update,
    Finish,
    Exit,
};

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MAIN;

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

    JsonObject root = JsonFile::LoadFromFile(configPath);
    if (root.IsEmpty())
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "game_config.json not found at '%s' — using defaults.", configPath.c_str());
        return cfg;
    }

    if (const std::string scene = root.GetString("startScene"); !scene.empty())
        cfg.startScene = scene;

    if (const double fps = root.GetDouble("targetFPS", 0.0); fps > 0.0)
        cfg.targetFPS = static_cast<float>(fps);

    NOUS_INFO_C(CURRENT_CHANNEL, "Loaded game_config.json: scene='%s', targetFPS=%.0f",
        cfg.startScene.c_str(), cfg.targetFPS);

    return cfg;
}

int main(int argc, char** argv)
{
    // Before everything, including the memory system. It matters more here than in
    // the editor: a shipped game has no console.log (InitializeLogging(false) below),
    // so on a player's machine the dump under Crashes/ is the only artifact.
    nous::engine::crash::InstallCrashHandler("GameApp");

    StartLogTimer();

    nous::engine::memory::InitializeMemory(MiB(300));

    nous::engine::multithreading::RegisterMainThread();

    InitializeLogging(false); // no console.log in game builds

    NOUS_INFO_C(CURRENT_CHANNEL, "Starting GameApp '%s'...", TITLE);

    int mainReturn = EXIT_FAILURE;
    auto state = GameState::Creation;
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

                App = NOUS_NEW<Application>(MemoryTag::APPLICATION, true);

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

                // Kick off async scene load. PressPlay is deferred until the scene has
                // actually been applied — see the IsLoadingScene() note in Update below.
                App->GetScene()->LoadSceneAsync(cfg.startScene);

                NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Update ----------");
                state = GameState::Update;
                break;
            }

            case GameState::Update:
            {
                // Once the async scene load completes, start simulation.
                //
                // This MUST be ModuleScene's own flag, not GetPendingJobs() == 0.
                // LoadSceneAsync's worker does the slow part and then hands the
                // registry mutation to the main thread via SubmitToMainThread, and
                // those tasks are deliberately NOT counted in mPendingJobs (counting
                // them would deadlock WaitForPendingJobs, which runs on the only
                // thread that drains them). So the job counter reaches 0 while the
                // scene is still an empty shell, and PressPlay would run against
                // nothing: every script then deserialized into an already-PLAYING
                // sim and never received Awake/Start.
                //
                // m_isLoadingScene is set before LoadSceneAsync returns and cleared
                // inside the main-thread task, after Deserialize + RefreshPrefabInstances,
                // so it covers the whole handoff. It also self-clears when the scene
                // path fails to resolve, so a bad startScene still reaches Update.
                if (!sceneReady && !App->GetScene()->IsLoadingScene())
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

    NOUS_INFO_C(CURRENT_CHANNEL, "---------- Application Destruction ----------");
    NOUS_DELETE(App, MemoryTag::APPLICATION);

    NOUS_INFO_C(CURRENT_CHANNEL, "GameApp exited successfully.");

    nous::engine::multithreading::UnregisterMainThread();

    NOUS_MULTILINE_C(LOG_LEVEL_INFO, CURRENT_CHANNEL,
        (std::string("[main] ") + nous::engine::memory::GetMemoryUsageStats()).c_str());

    ShutdownLogging();

    nous::engine::memory::ShutdownMemory();

    return mainReturn;
}