set(CLASS_NAME "ModuleResourceManager")

# ==============================================================================
# ModuleResourceManager -- converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/ModuleResourceManager/**  is the API, reached as <ModuleResourceManager/ModuleResourceManager.h>.
#   src/**                       is implementation, reachable only from inside this target.
#
# The one header is public: Application owns this module by value and the
# editor reaches it through EditorContext. No private headers today, so the
# src/ dir and the _private handle exist only to match every other target.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
        ModuleBase
        ResourceManager
        EventSystem_headers   # <Module>.h implements IEventListener
)

target_link_libraries(${CLASS_NAME} PRIVATE
        EventSystem
        Logger
        MemoryManager
        ModuleInput
        NOUS_Multithreading
        Utils
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()