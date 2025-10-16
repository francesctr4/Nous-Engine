#ifndef NOUS_ENGINE_EXPORT_H
#define NOUS_ENGINE_EXPORT_H

#ifdef _WIN32
#ifdef NOUS_ENGINE_EXPORTS
#define NOUS_ENGINE_API __declspec(dllexport)
#else
#define NOUS_ENGINE_API __declspec(dllimport)
#endif
#else
#define NOUS_ENGINE_API
#endif

#endif // NOUS_ENGINE_EXPORT_H
