# ============================================
# UnitTests.cmake
# ============================================

option(NOUS_BUILD_TESTS "Build Nous Engine module tests" ON)

if (NOUS_BUILD_TESTS)
    enable_testing()
endif()