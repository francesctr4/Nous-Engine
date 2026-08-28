#pragma once

// Three states, mirroring EngineCore/EngineExport.h -- see the reasoning there.
//
//   NOUS_EDITOR_EXPORTS  building the Nous-Editor DLL         -> dllexport
//   NOUS_EDITOR_STATIC   linking editor STATIC libs directly  -> nothing
//   (neither)            consuming the DLL                    -> dllimport
//
// The STATIC state is for narrowly-linked unit tests (t_GameExporter,
// t_InspectorRegistry), which link the one or two editor archives they use
// instead of the whole Nous-Editor DLL.

#ifdef _WIN32
#if defined(NOUS_EDITOR_STATIC)
#define NOUS_EDITOR_API
#elif defined(NOUS_EDITOR_EXPORTS)
#define NOUS_EDITOR_API __declspec(dllexport)
#else
#define NOUS_EDITOR_API __declspec(dllimport)
#endif
#else
#define NOUS_EDITOR_API
#endif
