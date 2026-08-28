#pragma once
#ifdef _WIN32
#ifdef NOUS_ENGINE_EXPORTS
#define NOUS_ENGINE_API __declspec(dllexport)
#else
#define NOUS_ENGINE_API __declspec(dllimport)
#endif
#else
#define NOUS_ENGINE_API
#endif

