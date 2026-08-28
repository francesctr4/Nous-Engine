set(CLASS_NAME "EventSystem")

# ==============================================================================
# EventSystem — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/EventSystem/**  is the API, reached as <EventSystem/EventSystem.h>.
#   src/**                  is implementation, reachable only from inside this target.
#
# All three headers are public: EventSystem.h and IEventListener.h are included
# by nine modules and the editor, and Event.h is reached THROUGH both of them
# (EventSystem::Emit takes an Event by value), so its own three direct includers
# never decided the question. This target buys no enforcement, which is a real
# outcome and not an oversight -- the same held for ShaderSystem and PrefabManager.
#
# include/ is flat: Event was a sub-unit dir holding exactly one public header,
# so <EventSystem/Event.h> beats <EventSystem/Event/Event.h>. src/ and test/
# keep the sub-unit dir and its CMakeLists.
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
        include/${CLASS_NAME}/IEventListener.h
        include/${CLASS_NAME}/Event.h
)

add_subdirectory(src/Event)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
)

target_link_libraries(${CLASS_NAME} PRIVATE Logger)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test/Event)
    add_subdirectory(test/EventSystem)
endif()
