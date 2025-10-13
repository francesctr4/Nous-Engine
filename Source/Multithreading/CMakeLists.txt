# ==============================
# Nous Engine - Multithreading
# ==============================

set(MULTITHREADING_SOURCES
        NOUS_Job.cpp
        NOUS_Job.h
        NOUS_JobSystem.cpp
        NOUS_JobSystem.h
        NOUS_Multithreading.cpp
        NOUS_Multithreading.h
        NOUS_Thread.cpp
        NOUS_Thread.h
        NOUS_ThreadPool.cpp
        NOUS_ThreadPool.h
)

add_library(Multithreading STATIC ${MULTITHREADING_SOURCES})
add_library(NousEngine::Multithreading ALIAS Multithreading)

set_target_properties(Multithreading PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Multithreading PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)