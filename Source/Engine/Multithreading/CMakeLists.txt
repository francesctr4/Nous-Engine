# ==============================
# Nous Engine - Multithreading
# ==============================

add_subdirectory(NOUS_Job)
add_subdirectory(NOUS_JobSystem)

set(MULTITHREADING_SOURCES
        NOUS_Multithreading/NOUS_Multithreading.cpp
        NOUS_Multithreading/NOUS_Multithreading.h
        NOUS_Thread/NOUS_Thread.cpp
        NOUS_Thread/NOUS_Thread.h
        NOUS_ThreadPool/NOUS_ThreadPool.cpp
        NOUS_ThreadPool/NOUS_ThreadPool.h
)

add_library(Multithreading STATIC ${MULTITHREADING_SOURCES})

set_target_properties(Multithreading PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Multithreading PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Multithreading PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Multithreading PUBLIC
        NOUS_Job
        NOUS_JobSystem
)

if(ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(Multithreading PUBLIC Tracy::TracyClient)
endif()