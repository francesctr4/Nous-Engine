# Nous Engine — Naming Conventions

C++23 standard. No Hungarian notation. Names should express **purpose**, not type.

---

## Types

| Kind | Convention | Example |
|---|---|---|
| Class / Struct | `PascalCase` | `ModuleResourceManager`, `ShaderCompileResult` |
| Interface (abstract base) | `I` prefix + `PascalCase` | `IImporterManager`, `IEventListener` |
| Enum class | `PascalCase` | `UpdateStatus`, `MemoryTag` |
| Enum class values | `PascalCase` | `UpdateStatus::Continue`, `MemoryTag::Texture` |
| Type alias (`using`) | `PascalCase` | `using UID = uint32_t;` |
| Concept | `PascalCase` | `Renderable`, `Importable` |
| Template parameter | `T` or `TPascal` | `T`, `TResource`, `TComponent` |

> **Never** use plain `enum` — always `enum class` to avoid name collisions and implicit conversions.

### Struct vs Class

- Use `struct` for **passive data** (no invariants, all members public): config objects, POD, return bundles.
- Use `class` for anything with **invariants**, private state, or non-trivial behavior.

```cpp
// struct — just data
struct ShaderCompilerConfig {
    bool optimizeSize = false;
    bool generateDebugInfo = true;
};

// class — has behavior and invariants
class ResourceMesh : public Resource { ... };
```

---

## Functions and Methods

All functions and methods use **`PascalCase`**, regardless of visibility.

```cpp
void Update(float dt);
ResourceMesh* GetDefaultMesh() const;
bool TryGetResource(UID uid, Resource** out_resource);
```

- Getters: `GetX()`
- Boolean queries: `IsX()`, `HasX()`, `CanX()`
- Output parameters: prefix with `out_` (prefer returning `std::optional` when practical)

### `const` correctness

- Mark methods `const` whenever they don't mutate observable state.
- Pass non-trivial types by `const&` unless you need a copy.
- Use `const` on local variables when the value won't change.

```cpp
const ResourceMesh* GetDefaultMesh() const;
void Load(const std::string& path);
const auto uid = GenerateUID();
```

---

## Variables

### Local variables
`camelCase`

```cpp
float deltaTime = 0.0f;
auto meshPath = std::string{};
uint32_t uid = 0;
```

### Private member variables
`m_camelCase`

```cpp
private:
    bool m_isGameMode;
    std::vector<Resource*> m_resources;
    std::mutex m_pendingUploadsMutex;
```

### Public member variables (POD/data structs only)
`camelCase` — no prefix

```cpp
struct ShaderCompilerConfig {
    bool optimizeSize = false;
    bool generateDebugInfo = true;
};
```

### Static class members
`s_camelCase`

```cpp
private:
    static Application* s_instance;
```

### Global variables
`g_camelCase` — and prefer to avoid mutable globals entirely.

```cpp
inline Application* g_App = nullptr;
```

---

## Constants

| Kind | Convention | Example |
|---|---|---|
| `constexpr` / `const` | `c_camelCase` | `c_defaultTargetFPS`, `c_maxFrames` |
| Macros (unavoidable) | `UPPER_SNAKE` with `NOUS_` prefix | `NOUS_ENGINE_API`, `NOUS_NEW` |

```cpp
static constexpr int c_defaultTargetFPS = 60;
static constexpr size_t c_maxTitleLength = 256;
```

> Prefer `constexpr` over `#define` for constants whenever possible.

---

## Macros

`NOUS_UPPER_SNAKE`. Use only when no language feature can replace the macro.

```cpp
#define NOUS_ENGINE_API __declspec(dllexport)
#define NOUS_NEW(type, tag) ...
```

---

## Namespaces

Fully lowercase, with `nous::` as the root namespace.

```cpp
namespace nous::threading { ... }
namespace nous::math { ... }
```

> The legacy `NOUS_Multithreading` namespace style is deprecated — migrate on touch.

---

## Files

| Kind | Convention | Example |
|---|---|---|
| Header | `PascalCase.h` | `ModuleResourceManager.h` |
| Source | `PascalCase.cpp` | `ModuleResourceManager.cpp` |
| Inline / template impl | `PascalCase.inl` | `ScriptRegistry.inl` |
| Test file | `t_PascalCase.cpp` | `t_ShaderCompiler.cpp` |
| Interface header | `iPascalCase.h` | `iRendererBackend.h` |

> **Interface naming:** the *type* keeps the capital-`I` prefix (`struct IRendererBackend`), but the *filename* uses a lowercase `i` (`iRendererBackend.h`) — stacked capitals like `IImporter.h` read poorly. Existing headers still use a capital `I`; migrate them to lowercase only when you touch them, don't mass-rename.

### Include guards

Always use `#pragma once`. No `#ifndef` header guards.

```cpp
#pragma once
```

---

## `auto` usage

Use `auto` when the type is obvious from the right-hand side or would be redundant:

```cpp
auto uid = GenerateUID();           // clear from function name
auto it = m_resources.find(uid);    // iterator — explicit type adds no value
auto* mesh = GetDefaultMesh();      // use auto* explicitly for pointers
```

Do **not** use `auto` when the type is not immediately obvious or matters to the reader:

```cpp
// Bad — what type is this?
auto result = Process(data);

// Good
ResourceMesh* result = Process(data);
```

---

## Null Pointers

Always use `nullptr`. Never `NULL` or `0` for pointers.

```cpp
// Bad
Resource* resource = NULL;
if (resource == 0) { ... }

// Good
Resource* resource = nullptr;
if (resource == nullptr) { ... }
```

---

## Casts

Always use C++ named casts. Never C-style casts.

| Cast | When to use |
|---|---|
| `static_cast<T>` | Safe, well-defined conversions (numeric, up/downcast in known hierarchy) |
| `reinterpret_cast<T>` | Bit-level reinterpretation (GPU buffers, raw memory) |
| `const_cast<T>` | Remove `const` — avoid unless interfacing with legacy C APIs |
| `dynamic_cast<T>` | Polymorphic downcast with runtime check — prefer static alternatives |

```cpp
// Bad
float dt = (float)deltaMs;

// Good
float dt = static_cast<float>(deltaMs);
```

---

## Virtual Methods

Always mark overriding methods with `override`. Never repeat `virtual` on overrides.

```cpp
// Bad
class ModuleRenderer3D : public Module {
    virtual bool Awake() { ... }
};

// Good
class ModuleRenderer3D : public Module {
    bool Awake() override { ... }
};
```

Use `final` on classes or methods that must not be further overridden.

---

## Target Layout: public `include/`, private `src/`

**Migration in progress.** Converted so far: `AudioSystem` (2026-08-26); the
foundation — `Logger`, `MemoryManager`, `FileSystem`, `NOUS_Multithreading` — all of
`Systems/` — `VideoSystem`, `ShaderSystem`, `CameraSystem`, `ECS`, `ResourceManager`,
`PrefabManager` — all four `Renderer/` targets, and the rest of the foundation —
`EventSystem`, `FileWatcher`, `TimeManager`, `Utils` (2026-08-27); all of `Modules/`
all of `Editor/` and `Scripting` (2026-08-28). Remaining: `Core` itself
(`Application`). Converted targets use:

```
Systems/AudioSystem/
├── include/AudioSystem/**   PUBLIC API   -> #include <AudioSystem/Foo.h>
├── src/**                   IMPLEMENTATION, unreachable from outside the target
│                            -> #include "AudioEngine/Backends/miniaudio/Bar.h"
└── test/**                  mirrors the sub-unit structure of src/
```

**Tests live at the target root, not under `src/`.** A test of a *public* header filed
under `src/` would contradict the distinction this layout exists to make, and keeping
tests out means `src/` is exactly "the code that ships". Per-leaf `CMakeLists.txt` are
kept in both `src/` and `test/`; `include/` has none, since the target root lists the
public headers in one block so the whole API is readable at a glance.

The target exposes `include/` PUBLIC and `src/` PRIVATE, so **a header under `src/` cannot
be included from another target — the build system enforces the boundary**, not a comment.
Deciding where a header goes *is* deciding whether it is API.

Each converted target also declares two include-dir-only `INTERFACE` handles:

- `<Target>_headers` — the public dir. The target links it PUBLIC, so ordinary consumers
  get it transitively and never name it. Add it to the engine DLL's PUBLIC link list too.
- `<Target>_private` — the implementation dir. **Only tests may link this**, and only to
  cover deliberately dependency-free policy code. It exists so such a test needs no engine
  objects at all: `t_GainDsp` links gtest plus this handle and nothing else.

Unconverted targets keep the old `unit/include|src|test` layout and `Engine/`-rooted
includes; `${CMAKE_SOURCE_DIR}/Source` stays on the include path until the last one is
converted.

**`Tools/check_header_layout.py` enforces the two rules below and runs in CI**
(before Configure, no build needed). Run it after any conversion or any new
`#include` of a converted header — it reports `MISSING_LINK` (a target compiles a
converted header with no link providing it), `PRIVATE_LEAK` (a target names a
converted target from a PUBLIC header while declaring the dependency PRIVATE, so
consumers break at a distance) and `PRIVATE_HEADER` (a header under `include/`
includes one under its own target's `src/`, which makes that header public in fact
while the layout claims otherwise). It prints the include chain behind each finding,
because these requirements are usually transitive and the right fix is often on the
intermediate target. Add each newly converted target to its `CONVERTED` table.

**A converted target's headers no longer resolve through the global `Source/` root.**
`<Logger/Logger.h>` resolves only via `Logger_headers`, so every consumer must declare
the link — which is the point, and is how the foundation conversion surfaced three
targets (`Utils`, `EditorUI`, `ModuleEditor`) that had been including Logger,
MemoryManager, FileSystem and the job system for free. Expect that on every conversion.
Targets linking `NousEngine::Engine` or `NousEngine::Editor` are covered automatically,
because the DLL's PUBLIC link list carries every converted target's `_headers` handle.

**A target is named after its own directory, because the target name is the include
prefix.** `CameraSystem/Camera/` defining a target called `Camera` would have produced
`<Camera/Camera.h>` — too generic a prefix to add to every consumer's include path — so
it was renamed and flattened to `Systems/CameraSystem/` on conversion.

**Mirror a folder per unit in `include/` where the units are a family you extend.**
`ResourceManager/Types/ResourceMesh/` and `ECS/Component/Types/CTransform/` exist in
`include/`, `src/` and `test/` under the same name, so adding a resource type or a
component stays "the three `X` folders" rather than a header filed away from its
sources. Apply this wherever there is a recurring add-one-more workflow; do **not**
apply it to one-off units (`ResourceManager/Core/`, `Runtime/`), where it only produces
`HotReloader/HotReloader.h`.

**Decide public/private by asking "is it reached through a public header?", not by
counting includers.** `ComponentList.h` (one includer) and `IImporterDispatcher.h`
(none outside its target) are both public, because `ComponentTypes.h` and
`IImporterManager.h` include them. Counting alone would have filed both under `src/`
and broken every consumer.

**Where a sub-unit contributes exactly one public header, `include/` is flat.**
`NOUS_JobSystem/NOUS_JobSystem.h` under `include/NOUS_Multithreading/` would be pure
repetition, so it is `<NOUS_Multithreading/NOUS_JobSystem.h>`. `src/` and `test/` still
keep the sub-unit directory (and its `CMakeLists.txt`) as the unit-boundary marker.
`AudioSystem` mirrors sub-unit dirs inside `include/` because its sub-units carry
several headers each; both are correct.

---

## Include Order

Within each group, sort alphabetically. Separate groups with a blank line.

```cpp
// 1. Own header (in .cpp files) — use shortest resolvable path from configured include dirs
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"

// 2. Engine headers -- converted targets use <Target/Header.h>, the rest are Engine/-rooted
#include <Logger/Logger.h>
#include "Engine/Systems/ResourceManager/ResourceBase.h"

// 3. Third-party headers
#include <assimp/Importer.hpp>
#include <imgui.h>

// 4. Standard library
#include <string>
#include <vector>
```

> This order ensures each header is self-sufficient — if an engine header is missing its own includes, the compiler will catch it immediately.

---

## Pointers and Ownership

Do **not** encode pointer or type information in names (no Hungarian notation).

```cpp
// Bad
Resource* p_resource = GetResource(uid);

// Good
Resource* resource = GetResource(uid);
Resource* pendingResource = GetResource(uid);  // disambiguate by purpose, not type
```

Express ownership through types, not names:
- Raw pointer `T*` → non-owning reference
- `std::unique_ptr<T>` → sole ownership
- `std::shared_ptr<T>` → shared ownership

---

## Summary Table

| Category | Convention |
|---|---|
| Types, interfaces, concepts | `PascalCase` |
| Methods / free functions | `PascalCase` |
| Local variables | `camelCase` |
| Private members | `m_camelCase` |
| Static members | `s_camelCase` |
| Globals | `g_camelCase` |
| Constants (`constexpr`) | `c_camelCase` |
| Macros | `NOUS_UPPER_SNAKE` |
| Enum class values | `PascalCase` |
| Namespaces | `nous::lower` |
| Template parameters | `T` / `TPascal` |
