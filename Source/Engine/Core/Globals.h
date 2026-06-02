#ifndef GLOBALS_H
#define GLOBALS_H

#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <limits>

constexpr auto TITLE = "Nous Engine";
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;
constexpr float DEFAULT_TARGET_FPS    = 144.00F;
constexpr float DEFAULT_SPIN_THRESHOLD = 0.002F;  // spin for the last 2ms before frame deadline

// ---------- Type Definitions ----------

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

// --------------------------------------

/**
 * @brief Any id set to this should be considered invalid
 * and not actually pointing to a real object.
 */
constexpr uint32 INVALID_ID = std::numeric_limits<uint32>::max();

// Gibibyte (2^30)
constexpr uint64 GiB(const uint64 amount)
{
    return amount * 1024ULL * 1024 * 1024;
}

// Mebibyte (2^20)
constexpr uint64 MiB(const uint64 amount)
{
    return amount * 1024ULL * 1024;
}

// Kibibyte (2^10)
constexpr uint64 KiB(const uint64 amount)
{
    return amount * 1024ULL;
}

// Gigabytes (10^9)
constexpr uint64 GB(const uint64 amount)
{
    return amount * 1000ULL * 1000 * 1000;
}

// Megabytes (10^6)
constexpr uint64 MB(const uint64 amount)
{
    return amount * 1000ULL * 1000;
}

// Kilobytes (10^3)
constexpr uint64 KB(const uint64 amount)
{
    return amount * 1000ULL;
}

// --------------------- Homemade Casts --------------------- //

// Ensures the value can be narrowed to a smaller type without losing data in the process.
// e.g., Converting from uint64 to uint8.
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
// e.g., Converting from int64 to uint8.
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

#endif // GLOBALS_H