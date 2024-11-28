#pragma once

// Handy MathGeoLib Includes
#include "MathGeoLib/include/Math/float4x4.h"
#include "MathGeoLib/include/Math/float4.h"
#include "MathGeoLib/include/Math/float3.h"

namespace NOUS_MathUtils 
{
    // Minimum and Maximum functions
    template <typename T>
    constexpr T MIN(const T& a, const T& b) {
        return (a < b) ? a : b;
    }

    template <typename T>
    constexpr T MAX(const T& a, const T& b) {
        return (a > b) ? a : b;
    }

    // Fundamental constants
    constexpr float PI = 3.14159265358979323846f; 
    constexpr float TAU = 6.2831853071795864769f;  // PI * 2
    constexpr float E = 2.718281828459045235360f;    

    // Angle conversions (Degree and Radians)
    constexpr float DEGTORAD = 0.0174532925199432957f;  // rad = deg * DEGTORAD
    constexpr float RADTODEG = 57.295779513082320876f;  // deg = rad * RADTODEG

    // Precomputed PI fractions
    constexpr float HALF_PI = 1.57079632679489661923f;          // PI / 2
    constexpr float QUARTER_PI = 0.78539816339744830962f;       // PI / 4
    constexpr float ONE_OVER_PI = 0.31830988618379067154f;      // 1 / PI
    constexpr float ONE_OVER_TWO_PI = 0.15915494309189533576f;  // 1 / (2 * PI)

    // Common square roots
    constexpr float SQRT_2 = 1.414213562373095048f;           // sqrt(2)
    constexpr float SQRT_3 = 1.732050807568877293f;           // sqrt(3)
    constexpr float SQRT_5 = 2.236067977499789696f;           // sqrt(5)
    constexpr float SQRT_1_OVER_2 = 0.70710678118654752440f;  // sqrt(1/2)
    constexpr float SQRT_1_OVER_3 = 0.57735026918962576450f;  // sqrt(1/3)

    // Logarithmic constants
    constexpr float LN_2 = 0.6931471805599453094f;     // ln(2)
    constexpr float LN_10 = 2.3025850929940456840f;    // ln(10)
    constexpr float LOG2_E = 1.4426950408889634073f;   // log2(e)
    constexpr float LOG10_E = 0.4342944819032518276f;  // log10(e)

    // Trigonometric constants
    constexpr float INV_PI = 0.318309886183790671f;        // 1 / PI
    constexpr float INV_SQRT_PI = 0.564189583547756286f;   // 1 / sqrt(PI)
    constexpr float TWO_OVER_PI = 0.6366197723675813430f;  // 2 / PI
    constexpr float FOUR_OVER_PI = 1.273239544735162686f;  // 4 / PI

    // Other mathematical constants
    constexpr float GOLDEN_RATIO = 1.61803398874989484820f;
    constexpr float GAMMA = 0.57721566490153286061f;
    constexpr float APERY = 1.20205690315959428540f;
    constexpr float CATALAN = 0.91596559417721901505f;

    // Useful limits
    constexpr float NOUS_INFINITY = 1e30f;
    constexpr float FLOAT_EPSILON = 1.192092896e-07f;

}