#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"

int main(int argc, char** argv)
{
    // Specify the amount of memory available for the project
    MemoryManager::InitializeMemory(MiB(300));

    NOUS_Multithreading::RegisterMainThread();

    InitializeLogging();

    NOUS_INFO("=== Started GameApp ===");

    NOUS_Multithreading::NOUS_Thread::SleepMS(2000);

    NOUS_Multithreading::UnregisterMainThread();

    NOUS_INFO(MemoryManager::GetMemoryUsageStats().c_str());

    NOUS_INFO("=== GameApp exited successfully ===");

    ShutdownLogging();

    MemoryManager::ShutdownMemory();

    return 0;
}