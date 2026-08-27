#pragma once

#ifdef _WIN32
#ifdef NOUS_EDITOR_EXPORTS
#define NOUS_EDITOR_API __declspec(dllexport)
#else
#define NOUS_EDITOR_API __declspec(dllimport)
#endif
#else
#define NOUS_EDITOR_API
#endif
