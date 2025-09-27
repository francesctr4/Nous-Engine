#ifndef NOUS_ENGINE_SCRIPTREGISTRYEXPORT_H
#define NOUS_ENGINE_SCRIPTREGISTRYEXPORT_H

#if defined(_WIN32) || defined(_WIN64)
#ifdef SCRIPTS_EXPORTS
#define SCRIPTS_API __declspec(dllexport)
#else
#define SCRIPTS_API __declspec(dllimport)
#endif
#else
#define SCRIPTS_API __attribute__((visibility("default")))
#endif

#endif //NOUS_ENGINE_SCRIPTREGISTRYEXPORT_H
