#include <Engine/Core/Globals.h>
#include <Engine/Core/Application.h>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include <Engine/Core/MemoryManager/MemoryManager.h>

// Editor
#include "Editor/ModuleEditor/include/ModuleEditor.h"

typedef enum MainState
{
    MAIN_START,
    MAIN_UPDATE,
    MAIN_FINISH,
    MAIN_EXIT

} MainState;

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_EDITOR_MAIN;

int main(/*int argc, char** argv*/)
{
    StartLogTimer(); // must be first — anchors all log timestamps to program start

    // Specify the amount of memory available for the project
    MemoryManager::InitializeMemory(MiB(50));

    NOUS_Multithreading::RegisterMainThread();

    InitializeLogging();

    NOUS_INFO_C(CURRENT_CHANNEL, "Starting engine '%s'....", TITLE);

    NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application Creation --------------");
    auto* App = NOUS_NEW<Application>(MemoryTag::APPLICATION);
    auto* Editor = NOUS_NEW<ModuleEditor>(MemoryTag::EDITOR,
        App->GetEventSystem(), App->GetJobSystem(),
        App->GetWindow(), App->GetInput(), App->GetCamera(),
        App->GetResourceManager(), App->GetScene(), App->GetRenderer());

    MainState nousState = MAIN_START;
    int mainReturn = EXIT_FAILURE;

    while (nousState != MAIN_EXIT)
    {
        switch (nousState)
        {
            case MAIN_START:

                NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application Awake --------------");

                if (!App->Awake())
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "Application Awake exits with ERROR");
                    nousState = MAIN_EXIT;
                }
                else
                {
                    // Run Editor Awake (optional)
                    if (Editor)
                        Editor->Awake();

                    NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application Start --------------");

                    if (!App->Start())
                    {
                        NOUS_ERROR_C(CURRENT_CHANNEL, "Application Start exits with ERROR");
                        nousState = MAIN_EXIT;
                    }
                    else
                    {
                        if (Editor)
                            Editor->Start();

                        nousState = MAIN_UPDATE;
                        NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application Update --------------");
                    }
                }

                break;

            case MAIN_UPDATE:
            {
                if (const UpdateStatus updateReturn = App->Update(); updateReturn == UpdateStatus::ERROR)
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "Application Update exits with ERROR");
                    nousState = MAIN_EXIT;
                }
                else if (updateReturn == UpdateStatus::STOP)
                {
                    nousState = MAIN_FINISH;
                }

                break;
            }

            case MAIN_FINISH:

                NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application CleanUp --------------");

                if (Editor)
                {
                    Editor->CleanUp();
                }

                if (App->CleanUp() == false)
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "Application CleanUp exits with ERROR");
                }
                else
                {
                    mainReturn = EXIT_SUCCESS;
                }

                nousState = MAIN_EXIT;

                break;

            default:;
        }
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "-------------- Application Destruction --------------");
    NOUS_DELETE(Editor, MemoryTag::EDITOR);
    NOUS_DELETE(App, MemoryTag::APPLICATION);

    NOUS_INFO_C(CURRENT_CHANNEL, "Exiting engine '%s'...", TITLE);

    NOUS_Multithreading::UnregisterMainThread();

    NOUS_MULTILINE_C(LOG_LEVEL_INFO, CURRENT_CHANNEL,
        (std::string("[main] ") + MemoryManager::GetMemoryUsageStats()).c_str());

    NOUS_INFO_C(CURRENT_CHANNEL, "Successfully exited Nous Engine. See you soon!");

    ShutdownLogging();

    MemoryManager::ShutdownMemory();

    return mainReturn;
}