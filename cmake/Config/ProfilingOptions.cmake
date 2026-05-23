# ============================================
# ProfilingOptions.cmake
# ============================================

if(ENABLE_PROFILING)
    message(STATUS "Profiling enabled: Tracy instrumentation active.")
    add_compile_definitions(_PROFILING TRACY_ENABLE NDEBUG)
endif()