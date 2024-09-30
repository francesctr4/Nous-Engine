#include "Application.h"
#include "Module.h"

#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "ModuleCamera3D.h"
#include "ModuleRenderer3D.h"

#include "Logger.h"
#include "ThreadPool.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    targetFPS = 1.0f / 60.0f;

	window = new ModuleWindow(this, "ModuleWindow");
	input = new ModuleInput(this, "ModuleInput");
    camera = new ModuleCamera3D(this, "ModuleCamera3D");
    renderer = new ModuleRenderer3D(this, "ModuleRenderer3D");

	list_modules[0] = window;
	list_modules[1] = input;
    list_modules[2] = camera;
    list_modules[3] = renderer;
}

Application::~Application()
{
	for (int i = 0; i < NUM_MODULES; ++i)
	{
		if (list_modules[i] != nullptr) {
			delete list_modules[i];
			list_modules[i] = nullptr;
		}
	}
}

bool Application::Awake()
{
    //bool ret = true;

    //// Call Awake() in all modules
    //for (int i = 0; i < NUM_MODULES && ret; ++i)
    //{
    //    if (list_modules[i] != nullptr) {
    //        ret = list_modules[i]->Awake();
    //    }
    //}

    //// After all Awake calls we call Start() in all modules
    //NOUS_INFO("-------------- Application Start --------------");
    //for (int i = 0; i < NUM_MODULES && ret; ++i)
    //{
    //    if (list_modules[i] != nullptr) {
    //        ret = list_modules[i]->Start();
    //    }
    //}

    //return ret;

    std::atomic<bool> result = true;

    // Submit each module's Awake() task to the ThreadPool
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        if (list_modules[i] != nullptr)
        {
            threadPool->Submit([this, i, &result]() {
                if (!list_modules[i]->Awake())
                {
                    result = false;
                }
                syncPoint.arrive_and_wait();  // Hit the barrier after each module completes
                });
        }
    }

    // Wait for all Awake() tasks to finish
    syncPoint.arrive_and_wait();  // Wait for all tasks to hit the barrier before continuing

    if (!result.load())  // If any module's Awake failed
        return false;

    // Now we move to the Start phase
    NOUS_INFO("-------------- Application Start --------------");

    // Submit each module's Start() task to the ThreadPool
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        if (list_modules[i] != nullptr)
        {
            threadPool->Submit([this, i, &result]() {
                if (!list_modules[i]->Start())
                {
                    result = false;
                }
                syncPoint.arrive_and_wait();  // Hit the barrier after Start finishes
                });
        }
    }

    // Wait for all Start() tasks to finish
    syncPoint.arrive_and_wait();

    return result.load();
}

// Barrier to synchronize all threads at the end of each phase
std::barrier sync_point(NUM_MODULES, []() noexcept {
    // This completion function is called once all threads reach the barrier
    // You can use this to check for exit conditions after each phase if needed.
    NOUS_DEBUG("%s", "Barrier Surpassed");
    });

UpdateStatus Application::Update()
{
    UpdateStatus ret = UPDATE_CONTINUE;

    ret = PrepareUpdate();

    //std::vector<std::thread> threads;

    //for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i) {
    //    if (list_modules[i] != nullptr) {
    //        // Create a new thread to run the PreUpdate function
    //        threads.emplace_back([&, i]() {
    //            ret = list_modules[i]->PreUpdate(targetFPS);
    //            });
    //    }
    //}

    //// Join all threads to ensure they complete before moving forward
    //for (std::thread& t : threads) {
    //    if (t.joinable()) {
    //        t.join();
    //    }
    //}

    /*for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PreUpdate(targetFPS);
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Update(targetFPS);
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PostUpdate(targetFPS);
        }
    }*/

    // Worker function that handles the module's tasks for each phase
    auto WorkerFunction = [&](int moduleIndex) {
        if (list_modules[moduleIndex] != nullptr) {
            // PreUpdate phase
            if (ret == UPDATE_CONTINUE) {
                ret = list_modules[moduleIndex]->PreUpdate(targetFPS);
            }
            sync_point.arrive_and_wait(); // Wait for all threads to finish PreUpdate

            // Update phase
            if (ret == UPDATE_CONTINUE) {
                ret = list_modules[moduleIndex]->Update(targetFPS);
            }
            sync_point.arrive_and_wait(); // Wait for all threads to finish Update

            // PostUpdate phase
            if (ret == UPDATE_CONTINUE) {
                ret = list_modules[moduleIndex]->PostUpdate(targetFPS);
            }
            sync_point.arrive_and_wait(); // Wait for all threads to finish PostUpdate
        }
        };

    // Create and launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_MODULES; ++i) {
        if (list_modules[i] != nullptr) {
            threads.emplace_back(WorkerFunction, i);
        }
    }

    // Join all threads to ensure they complete before moving forward
    for (std::thread& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    FinishUpdate();

    return ret;
}

bool Application::CleanUp()
{
    bool ret = true;
    for (int i = (NUM_MODULES - 1); i >= 0 && ret; --i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->CleanUp();
        }
    }
    return ret;
}

UpdateStatus Application::PrepareUpdate()
{
    UpdateStatus ret = UPDATE_CONTINUE;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
        {
            ret = UPDATE_STOP;
            break;
        }
        }
    }

    return ret;
}

void Application::FinishUpdate()
{

}