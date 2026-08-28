#pragma once

// Three states, not two.
//
//   NOUS_ENGINE_EXPORTS  building the NousEngine DLL          -> dllexport
//   NOUS_ENGINE_STATIC   linking engine STATIC libs directly  -> nothing
//   (neither)            consuming the DLL                    -> dllimport
//
// The STATIC state exists for narrowly-linked unit tests, which link the two or
// three engine archives they actually use instead of the whole DLL. Without it a
// test TU emits __imp_-prefixed references, and the linker will NOT pull an
// archive member in solely to satisfy one. Whether a given symbol resolved then
// came down to whether its object had been loaded for some other reason:
// CCamera::GetProjectionMatrix linked (with LNK4217) while Scene::CreateGameObject
// did not, from the same header and the same caller. Tests that link static libs
// must define NOUS_ENGINE_STATIC so the reference is plain and always resolves.
//
// dllexport on the definition does not change the decorated name, only the export
// directive, so a plain reference matches an archive built with EXPORTS.

#ifdef _WIN32
#if defined(NOUS_ENGINE_STATIC)
#define NOUS_ENGINE_API
#elif defined(NOUS_ENGINE_EXPORTS)
#define NOUS_ENGINE_API __declspec(dllexport)
#else
#define NOUS_ENGINE_API __declspec(dllimport)
#endif
#else
#define NOUS_ENGINE_API
#endif
