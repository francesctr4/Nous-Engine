#pragma once

#include <entt/entt.hpp>
#include <string_view>

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

// Compile-time list of component types — the single mechanism behind every
// generic enumerate / construct / remove / teardown site in the ECS.
// EnTT is statically typed, so each operation is a fold expression over Ts.
//
// To register a new component type, add it to the `ComponentTypes` alias in
// ComponentTypes.h — never edit per-type dispatch by hand.
template <typename... Ts>
struct ComponentList
{
    // Invoke f(Component*) for each listed type attached to `e`, in declaration
    // order. Absent types are skipped.
    template <typename F>
    static void ForEachPresent(entt::registry& reg, entt::entity e, F&& f)
    {
        ([&] {
            if (auto* c = reg.try_get<Ts>(e))
                f(static_cast<Component*>(c));
        }(), ...);
    }

    // Invoke f.operator()<T>() for every listed type, in declaration order.
    // Call with an explicit-template lambda: ForEachType([]<typename T>(){ ... }).
    template <typename F>
    static void ForEachType(F&& f)
    {
        (f.template operator()<Ts>(), ...);
    }

    // True if `name` matches one of the listed component types' TypeName.
    // Pure name check — constructs nothing, runs no OnStart.
    static bool IsKnownName(std::string_view name)
    {
        return ((name == Ts::TypeName) || ...);
    }

    // Attach the component whose TypeName == `name` to `go` and return it
    // (caller may then Deserialize). Returns nullptr if no type matches.
    static Component* AddByName(GameObject& go, std::string_view name)
    {
        Component* result = nullptr;
        (void)((name == Ts::TypeName
                    ? (result = &go.AddComponent<Ts>(), true)
                    : false) || ...);
        return result;
    }

    // Remove the component whose TypeName == `name` from `go`.
    // Returns true if a listed type matched the name.
    static bool RemoveByName(GameObject& go, std::string_view name)
    {
        bool matched = false;
        (void)((name == Ts::TypeName
                    ? (go.RemoveComponent<Ts>(), matched = true)
                    : false) || ...);
        return matched;
    }
};
