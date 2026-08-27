#include <NOUS_Multithreading/NOUS_Multithreading.h>
#include <NOUS_Multithreading/NOUS_Thread.h>

#include <MemoryManager/MemoryManager.h>

namespace nous::engine::multithreading
{
    NOUS_Thread* sMainThread = nullptr;

    /// @brief Initializes the main thread tracking.
    /// @note Must be paired with UnregisterMainThread() to prevent memory leaks.
    void RegisterMainThread()
    {
        if (!sMainThread)
        {
            sMainThread = NOUS_NEW<NOUS_Thread>(MemoryTag::THREAD);

            sMainThread->SetName("Main Thread");
            sMainThread->SetThreadID(std::this_thread::get_id());
            sMainThread->SetThreadState(ThreadState::RUNNING);
            sMainThread->StartExecutionTimer();
        }
    }

    /// @brief Deletes the main thread instance if it exists.
    /// @note Should be called during application shutdown.
    void UnregisterMainThread()
    {
        if (sMainThread)
        {
            NOUS_DELETE(sMainThread, MemoryTag::THREAD);
        }
    }

    /// @brief Retrieves the main thread instance.
    /// @return Pointer to the main thread object, or nullptr if not registered.
    NOUS_Thread* GetMainThread()
    {
        return sMainThread;
    }

    /// @return true when the calling thread is the registered main thread.
    /// @note Permissive when no main thread is registered - see the header.
    bool IsOnMainThread()
    {
        const NOUS_Thread* mainThread = GetMainThread();
        return mainThread == nullptr || mainThread->GetID() == std::this_thread::get_id();
    }
}
