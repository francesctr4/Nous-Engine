#ifndef ASSERTS_H
#define ASSERTS_H

#include <EngineCore/EngineExport.h>

// Disable assertions by commenting out the below line.
#define NOUS_ASSERTIONS_ENABLED

#ifdef NOUS_ASSERTIONS_ENABLED

// Platform-specific debug break
#if defined(_MSC_VER)
    #include <intrin.h>
    #define NOUS_DebugBreak() __debugbreak()
#elif defined(__unix__) || defined(__APPLE__)
    #include <csignal>
    #define NOUS_DebugBreak() raise(SIGTRAP)
#else
    #define NOUS_DebugBreak() ((void)0) // Fallback: no-op
#endif

#include <cstdint>

NOUS_ENGINE_API void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line);

#define NOUS_ASSERT(expr)                                                \
    {                                                                    \
        if (!(expr)) {                                                   \
            ReportAssertionFailure(#expr, "", __FILE__, __LINE__);       \
            NOUS_DebugBreak();                                           \
        }                                                                \
    }

#define NOUS_ASSERT_MSG(expr, message)                                   \
    {                                                                    \
        if (!(expr)) {                                                   \
            ReportAssertionFailure(#expr, message, __FILE__, __LINE__);  \
            NOUS_DebugBreak();                                           \
        }                                                                \
    }

#ifdef _DEBUG
#define NOUS_ASSERT_DEBUG(expr)                                          \
    {                                                                    \
        if (!(expr)) {                                                   \
            ReportAssertionFailure(#expr, "", __FILE__, __LINE__);       \
            NOUS_DebugBreak();                                           \
        }                                                                \
    }
#else
#define NOUS_ASSERT_DEBUG(expr)
#endif // _DEBUG

// Structural mutation of the entt registry is main-thread-only. See CLAUDE.md
// "Threading: Editor-Observes-Engine Rule". Debug-only: this is a guard rail for
// development, and it matters most on Windows, where ThreadSanitizer does not exist.
#ifdef _DEBUG
    #include <NOUS_Multithreading/NOUS_Multithreading.h>
    #define NOUS_ASSERT_MAIN_THREAD()                                                 \
        NOUS_ASSERT_MSG(nous::engine::multithreading::IsOnMainThread(),               \
            "Registry mutation from a non-main thread. Wrap it in "                   \
            "jobSystem->SubmitToMainThread(...) - see the main-thread ECS design doc.")
#else
    #define NOUS_ASSERT_MAIN_THREAD() ((void)0)
#endif // _DEBUG

#else // !NOUS_ASSERTIONS_ENABLED

#define NOUS_ASSERT(expr)
#define NOUS_ASSERT_MSG(expr, message)
#define NOUS_ASSERT_DEBUG(expr)
#define NOUS_ASSERT_MAIN_THREAD()

#endif // NOUS_ASSERTIONS_ENABLED

#endif // ASSERTS_H