set(CLASS_NAME "TimeManager")

# ==============================================================================
# TimeManager — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/TimeManager/**  is the API, reached as <TimeManager/TimeManager.h>.
#   src/**                  is implementation, reachable only from inside this target.
#
# Both headers are public. Timer.h has ZERO direct includers outside this target,
# which reads as "private" until you look at why: TimeManager.h holds Timers by
# value, so every consumer of the public header compiles Timer.h. A header
# reached THROUGH a public header is public, whatever the count says -- this is
# the same trap that misfiled ComponentList.h, IImporterDispatcher.h and
# GroupGeometries.h, and Tools/check_header_layout.py now fails the build on it.
#
# include/ is flat: Timer was a sub-unit dir holding exactly one public header,
# so <TimeManager/Timer.h> beats <TimeManager/Timer/Timer.h>. src/ and test/
# keep the sub-unit dir and its CMakeLists.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

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
        include/${CLASS_NAME}/Timer.h
)

add_subdirectory(src/Timer)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# TimeManager depends on nothing but <chrono>; it declared no links before the
# conversion either, and that was accurate rather than an omission.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test/Timer)
endif()
