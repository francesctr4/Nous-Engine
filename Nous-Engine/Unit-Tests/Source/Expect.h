#pragma once

#include "Logger.h"
#include <cmath>

/**
 * @brief Expects expected to be equal to actual.
 */
#define EXPECT_SHOULD_BE(expected, actual)                                                                  \
    if (actual != expected) {                                                                               \
        NOUS_ERROR("--> Expected %lld, but got: %lld. File: %s:%d.", expected, actual, __FILE__, __LINE__); \
        return false;                                                                                       \
    }
 /**
  * @brief Expects expected to NOT be equal to actual.
  */
#define EXPECT_SHOULD_NOT_BE(expected, actual)                                                                       \
    if (actual == expected) {                                                                                        \
        NOUS_ERROR("--> Expected %d != %d, but they are equal. File: %s:%d.", expected, actual, __FILE__, __LINE__); \
        return false;                                                                                                \
    }
  /**
   * @brief Expects expected to be actual given a tolerance of K_FLOAT_EPSILON.
   */
#define EXPECT_FLOAT_TO_BE(expected, actual)                                                            \
    if (std::abs(expected - actual) > 0.001f) {                                                         \
        NOUS_ERROR("--> Expected %f, but got: %f. File: %s:%d.", expected, actual, __FILE__, __LINE__); \
        return false;                                                                                   \
    }
   /**
    * @brief Expects actual to be true.
    */
#define EXPECT_TO_BE_TRUE(actual)                                                           \
    if (actual != true) {                                                                   \
        NOUS_ERROR("--> Expected true, but got: false. File: %s:%d.", __FILE__, __LINE__);  \
        return false;                                                                       \
    }
    /**
     * @brief Expects actual to be false.
     */
#define EXPECT_TO_BE_FALSE(actual)                                                          \
    if (actual != false) {                                                                  \
        NOUS_ERROR("--> Expected false, but got: true. File: %s:%d.", __FILE__, __LINE__);  \
        return false;                                                                       \
    }