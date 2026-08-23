# ============================================
# UnitTests.cmake
# ============================================

option(NOUS_BUILD_TESTS "Build Nous Engine module tests" ON)

# Valgrind memcheck is OFF by default and should stay that way for routine work.
# It runs the whole suite at roughly 20-50x, and because MemoryManager hands out
# every NOUS_NEW from a single ~50 MB pooled malloc, memcheck cannot see overflows,
# use-after-free or leaks *between* engine objects at all — it only inspects raw
# allocations made by gtest, libstdc++ and third-party code. The cost/benefit only
# turns positive once DynamicAllocator/FreeList carry VALGRIND_MEMPOOL_* annotations.
#
# Enable deliberately, for a targeted run:
#   cmake -S . -B Build/Debug-Linux -DNOUS_ENABLE_MEMCHECK=ON
#   cmake --build Build/Debug-Linux --target memcheck
#   ctest -T memcheck -R t_FreeList --output-on-failure    # or one target at a time
option(NOUS_ENABLE_MEMCHECK "Wire up Valgrind memcheck for CTest (slow; off by default)" OFF)

if (NOUS_BUILD_TESTS)

    if (NOUS_ENABLE_MEMCHECK)

        # These MUST be set before include(CTest): CTest is what writes
        # DartConfiguration.tcl, and `ctest -T memcheck` reads the memory-checker
        # settings from it. A plain enable_testing() never generates that file, and
        # memcheck then fails with "Memory checker (MemoryCheckCommand) not set".
        #
        # --error-exitcode=1 is required: without it Valgrind exits 0 even when it
        # finds errors, so CTest would report every test as passing.
        #
        # --track-origins=yes roughly doubles the runtime. Drop it first if you need
        # the run to finish sooner and don't need origins of uninitialised values.
        find_program(MEMORYCHECK_COMMAND valgrind)

        if (MEMORYCHECK_COMMAND)
            set(MEMORYCHECK_COMMAND_OPTIONS
                    "--leak-check=full --show-leak-kinds=definite --track-origins=yes --error-exitcode=1")
            set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/Tools/Sanitizers/valgrind.supp")

            set(BUILD_TESTING ON CACHE BOOL "" FORCE)
            include(CTest)  # calls enable_testing() and emits DartConfiguration.tcl

            add_custom_target(memcheck
                    COMMAND ${CMAKE_CTEST_COMMAND} -T memcheck --output-on-failure
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running all tests under Valgrind memcheck (slow)"
                    USES_TERMINAL)
        else()
            message(WARNING "NOUS_ENABLE_MEMCHECK=ON but valgrind was not found; memcheck disabled.")
            enable_testing()
        endif()

    else()
        enable_testing()
    endif()

endif()
