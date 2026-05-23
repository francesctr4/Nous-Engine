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
| Interface header | `IPascalCase.h` | `IImporterManager.h` |

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

## Include Order

Within each group, sort alphabetically. Separate groups with a blank line.

```cpp
// 1. Own header (in .cpp files) — use shortest resolvable path from configured include dirs
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"

// 2. Engine headers
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/ResourceManager/Resource.h"

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
