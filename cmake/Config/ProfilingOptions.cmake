# ============================================
# ProfilingOptions.cmake
# ============================================

option(ENABLE_PROFILING "Enable profiling instrumentation (Tracy, etc.)" OFF)

if(ENABLE_PROFILING)
    message(STATUS "Profiling enabled: Tracy instrumentation active.")
    add_compile_definitions(_PROFILING NDEBUG)
endif()