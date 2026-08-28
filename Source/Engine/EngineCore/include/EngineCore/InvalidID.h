#pragma once

#include <cstdint>
#include <limits>

/**
 * @brief Any id set to this should be considered invalid
 * and not actually pointing to a real object.
 */
constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();
