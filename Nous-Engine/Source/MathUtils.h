#pragma once

namespace NOUS_MathUtils 
{
#define MIN(a,b) ((a)<(b)) ? (a) : (b)
#define MAX(a,b) ((a)>(b)) ? (a) : (b)

    constexpr double PI = 3.14159265358979323846264338327950288;    // PI
    constexpr double TAU = 6.28318530717958647692528676655900576;   // 2 * PI
    constexpr double e = 2.71828182845904523536028747135266249;     // Euler's number

    constexpr double DEGTORAD = 0.0174532925199432957;
    constexpr double RADTODEG = 57.295779513082320876;
}