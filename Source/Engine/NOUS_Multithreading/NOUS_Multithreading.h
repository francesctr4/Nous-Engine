#pragma once

#include "Engine/EngineExport.h"

namespace nous::engine::multithreading
{
    // Forward declaration.
    class NOUS_Thread;

    /// @brief Global pointer to the main thread instance.
    /// @note Initialized by RegisterMainThread() and cleaned up by UnregisterMainThread().
    extern NOUS_Thread* sMainThread;

    /// @brief Initializes the main thread tracking.
    /// @note Must be paired with UnregisterMainThread() to prevent leaks.
    NOUS_ENGINE_API void RegisterMainThread();

    /// @brief Deletes the main thread instance if it exists.
    /// @note Should be called during application shutdown.
    NOUS_ENGINE_API void UnregisterMainThread();

    /// @brief Retrieves the main thread instance.
    /// @return Pointer to the main thread object, or nullptr if not registered.
    NOUS_ENGINE_API NOUS_Thread* GetMainThread();

    /// @return true when the calling thread is the registered main thread.
    /// @note Returns TRUE when no main thread has been registered at all. Unit-test
    ///       binaries never call RegisterMainThread(), and a test that never spawns a
    ///       worker cannot violate the main-thread invariant - so the permissive answer
    ///       is the correct one there, and test code stays unaware the rule exists.
    NOUS_ENGINE_API bool IsOnMainThread();
}