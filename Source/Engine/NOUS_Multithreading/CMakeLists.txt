# ==============================================================================
# Nous Engine - NOUS_Multithreading
#
# Converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/NOUS_Multithreading/**  is the API, reached as
#                                   <NOUS_Multithreading/NOUS_JobSystem.h>.
#   src/**                          is implementation.
#
# All five headers are public: every one of Job / JobSystem / Thread / ThreadPool
# / the facade has consumers outside this target (the job system alone has 19).
# There is no private header today; src/ and the _private handle exist so the
# shape matches every other converted target.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

set(CLASS_NAME "NOUS_Multithreading")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
        include/${CLASS_NAME}/NOUS_Job.h
        include/${CLASS_NAME}/NOUS_JobSystem.h
        include/${CLASS_NAME}/NOUS_Thread.h
        include/${CLASS_NAME}/NOUS_ThreadPool.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
)

if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${CLASS_NAME} PRIVATE Tracy::TracyClient)
endif()

add_subdirectory(src/NOUS_Job)
add_subdirectory(src/NOUS_Thread)
add_subdirectory(src/NOUS_ThreadPool)
add_subdirectory(src/NOUS_JobSystem)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
