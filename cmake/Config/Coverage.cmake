# ============================================
# Coverage.cmake — GCC/Clang line-coverage wiring
# ============================================
#
#   cmake --preset Coverage-Linux   -> NOUS_COVERAGE=ON
#   cmake --build Build/Coverage-Linux
#   ctest --test-dir Build/Coverage-Linux --output-on-failure
#   Tools/Coverage/report.sh Build/Coverage-Linux
#
# Reports LINE coverage of Source/Engine, measured by running the test suite.
#
# Why GCC/Clang only: MSVC has no free instrumentation of this kind. On Windows
# use OpenCppCoverage against an already-built test binary instead -- it needs no
# build flags at all:
#
#   OpenCppCoverage --sources Source\Engine --export_type=html:cov -- ^
#       Build\Debug-Windows\bin\t_ECS_Scene.exe
#
# Flags are applied GLOBALLY, for the same reason the sanitizers are: the engine
# is a shared library and a partially instrumented process produces a report that
# silently omits whatever was not instrumented.
#
# Caveats worth knowing before trusting a number:
#   * Optimisation is forced off. -O2 merges and reorders lines until the mapping
#     back to source is fiction.
#   * vcpkg dependencies are NOT instrumented, which is what we want -- the
#     number should describe this engine, not gtest or glm.
#   * Header-only code (the policy headers, NOUS_Vector, the registries) is
#     counted at every point it is INCLUDED, so a widely-included header can move
#     the total more than its size suggests.

option(NOUS_COVERAGE "Instrument for line coverage (GCC/Clang only). Forces -O0 -g." OFF)

if(NOUS_COVERAGE)
    if(MSVC)
        message(WARNING "NOUS_COVERAGE ignored: use OpenCppCoverage on MSVC (see Coverage.cmake).")
        set(NOUS_COVERAGE OFF CACHE BOOL "" FORCE)
    else()
        # --coverage is the portable spelling of -fprofile-arcs -ftest-coverage and
        # works on both GCC (gcov) and Clang (llvm-cov gcov-compatible mode).
        # -fprofile-update=atomic is REQUIRED here, not a refinement. gcov counters
        # are plain increments by default, and this suite runs genuinely concurrent
        # tests (t_ResourceManager_ResourceBase spawns 8 threads over one refcount;
        # the NOUS_Multithreading tests hammer the job system). Concurrent updates
        # lose increments and can drive a counter NEGATIVE, at which point lcov
        # aborts with "Unexpected negative count" rather than reporting anything.
        # Atomic counters cost runtime but make the numbers trustworthy.
        add_compile_options(--coverage -fprofile-update=atomic
                            -O0 -g -fno-inline -fno-elide-constructors)
        add_link_options(--coverage)

        message(STATUS "Coverage ENABLED: --coverage (-O0, inlining off)")
    endif()
endif()
