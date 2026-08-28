#pragma once

#include <cassert>
#include <limits>
#include <stdexcept>

// Homemade casts. Moved here from Globals.h, which mixed them with app config,
// memory-size literals and an id sentinel.
//
// narrow_cast and safe_cast currently have no callers engine-wide; they are kept
// deliberately as part of the intended cast vocabulary, not left behind by
// accident. Do not "clean them up".

// Ensures the value can be narrowed to a smaller type without losing data in the process.
// e.g., Converting from uint64_t to uint8_t.
template<typename Target, typename Source>
auto narrow_cast(Source v)
{
    auto r = static_cast<Target>(v);

    if (static_cast<Source>(r) != v)
    {
        throw std::runtime_error("narrow_cast failed");
    }

    return r;
}

// Ensures the value fits within the limits of the target type to prevent overflows or underflows.
// Useful to throw an error if we are casting a negative number to uint.
// e.g., Converting from int64_t to uint8_t.
template <typename Target, typename Source>
auto safe_cast(Source value)
{
    if (value < std::numeric_limits<Target>::min() || value > std::numeric_limits<Target>::max())
    {
        throw std::runtime_error("safe_cast failed");
    }

    return static_cast<Target>(value);
}

// Safe downcasting utility for polymorphic types (pointers only)
// e.g., Converting from ResourceBase* to ResourceMesh*.
template <typename Target, typename Source>
auto down_cast(Source ptr)
{
    assert(dynamic_cast<Target>(ptr) != nullptr && "down_cast failed");

    return static_cast<Target>(ptr);
}
