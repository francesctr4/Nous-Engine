#ifndef ASSERTS_H
#define ASSERTS_H

#include "Core/Globals.h"

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

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line);

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

#else // !NOUS_ASSERTIONS_ENABLED

#define NOUS_ASSERT(expr)
#define NOUS_ASSERT_MSG(expr, message)
#define NOUS_ASSERT_DEBUG(expr)

#endif // NOUS_ASSERTIONS_ENABLED

#endif // ASSERTS_H