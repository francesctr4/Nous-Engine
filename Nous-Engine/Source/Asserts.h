#pragma once

#include "Globals.h"

// Disable assertions by commenting out the below line.
#define NOUS_ASSERTIONS_ENABLED

#ifdef NOUS_ASSERTIONS_ENABLED
#include <intrin.h>
#define DebugBreak() __debugbreak()

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line);

#define NOUS_ASSERT(expr)												\
	{																	\
		if (expr) {														\
		} else {														\
			ReportAssertionFailure(#expr, "", __FILE__, __LINE__);		\
			DebugBreak();												\
		}																\
	}																	\

#define NOUS_ASSERT_MSG(expr, message)									\
	{																	\
		if (expr) {														\
		} else {														\
			ReportAssertionFailure(#expr, message, __FILE__, __LINE__);	\
			DebugBreak();												\
		}																\
	}																	\

#ifdef _DEBUG
#define NOUS_ASSERT_DEBUG(expr)											\
	{																	\
		if (expr) {														\
		} else {														\
			ReportAssertionFailure(#expr, "", __FILE__, __LINE__);		\
			DebugBreak();												\
		}																\
	}																	
#else
// Does nothing at all
#define NOUS_ASSERT_DEBUG(expr)
#endif // _DEBUG

#else

// Does nothing at all
#define NOUS_ASSERT(expr)
#define NOUS_ASSERT_MSG(expr, message)	
#define NOUS_ASSERT_DEBUG(expr)

#endif // NOUS_ASSERTIONS_ENABLED