#include <Engine/Core/Globals.h>
#include <Engine/Core/Application.h>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include <Engine/Core/MemoryManager/MemoryManager.h>

// Editor
#include "Editor/ModuleEditor/include/ModuleEditor.h"

typedef enum MainState
{
    MAIN_CREATION,
    MAIN_START,
    MAIN_UPDATE,
    MAIN_FINISH,
    MAIN_EXIT

} MainState;

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_EDITOR_MAIN;

int main(int argc, char** argv)
{
    // Specify the amount of memory available for the project
    MemoryManager::InitializeMemory(MiB(50));

    NOUS_Multithreading::RegisterMainThread();

    InitializeLogging();

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Starting engine '%s'....", __FUNCTION__, TITLE);

    int mainReturn = EXIT_FAILURE;
    MainState nousState = MAIN_CREATION;
    Application* App = nullptr;
    ModuleEditor* Editor = nullptr;

    while (nousState != MAIN_EXIT)
    {
        switch (nousState)
        {
            case MAIN_CREATION:

                NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application Creation --------------", __FUNCTION__);
                App = NOUS_NEW<Application>(MemoryTag::APPLICATION);
                Editor = NOUS_NEW<ModuleEditor>(MemoryTag::EDITOR, App);

                nousState = MAIN_START;

                break;

            case MAIN_START:

                NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application Awake --------------", __FUNCTION__);

                if (App && !App->Awake())
                {
                    NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Application Awake exits with ERROR", __FUNCTION__);
                    nousState = MAIN_EXIT;
                }
                else
                {
                    // Run Editor Awake (optional)
                    if (Editor)
                        Editor->Awake();

                    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application Start --------------", __FUNCTION__);

                    if (App && !App->Start())
                    {
                        NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Application Start exits with ERROR", __FUNCTION__);
                        nousState = MAIN_EXIT;
                    }
                    else
                    {
                        if (Editor)
                            Editor->Start();

                        nousState = MAIN_UPDATE;
                        NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application Update --------------", __FUNCTION__);
                    }
                }

                break;

            case MAIN_UPDATE:
            {
                UpdateStatus updateReturn = App->Update();

                if (updateReturn == UpdateStatus::ERROR)
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Application Update exits with ERROR", __FUNCTION__);
                    nousState = MAIN_EXIT;
                }
                else if (updateReturn == UpdateStatus::STOP)
                {
                    nousState = MAIN_FINISH;
                }

                break;
            }

            case MAIN_FINISH:

                NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application CleanUp --------------", __FUNCTION__);

                if (Editor)
                {
                    Editor->CleanUp();
                }

                if (App->CleanUp() == false)
                {
                    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Application CleanUp exits with ERROR", __FUNCTION__);
                }
                else
                {
                    mainReturn = EXIT_SUCCESS;
                }

                nousState = MAIN_EXIT;

                break;

        }
    }

    External = nullptr;

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] -------------- Application Destruction --------------", __FUNCTION__);
    NOUS_DELETE(Editor, MemoryTag::EDITOR);
    NOUS_DELETE(App, MemoryTag::APPLICATION);

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Exiting engine '%s'...", __FUNCTION__, TITLE);

    NOUS_Multithreading::UnregisterMainThread();

    NOUS_INFO_C(CURRENT_CHANNEL, (std::string("[%s] ") + MemoryManager::GetMemoryUsageStats()).c_str(), __FUNCTION__);

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Successfully exited Nous Engine. See you soon!", __FUNCTION__);

    ShutdownLogging();

    MemoryManager::ShutdownMemory();

    return mainReturn;
}