# ==============================
# Nous Engine - NOUS_Multithreading
# ==============================

set(CLASS_NAME "NOUS_Multithreading")

add_library(${CLASS_NAME} STATIC
        ${CLASS_NAME}.cpp
        ${CLASS_NAME}.h
)

add_subdirectory(NOUS_Job)
add_subdirectory(NOUS_Thread)
add_subdirectory(NOUS_ThreadPool)
add_subdirectory(NOUS_JobSystem)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${CLASS_NAME} PRIVATE Tracy::TracyClient)
endif()