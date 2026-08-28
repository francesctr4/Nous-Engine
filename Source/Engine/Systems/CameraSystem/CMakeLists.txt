set(CLASS_NAME "CameraSystem")

# ==============================================================================
# CameraSystem — public/private header layout (see CONVENTIONS.md).
#
#   include/CameraSystem/**  is the API, reached as <CameraSystem/Camera.h>.
#   src/**                   is implementation.
#
# The target was named `Camera` and lived one level down at CameraSystem/Camera/.
# It is renamed here because the target name becomes the include prefix, and
# <Camera/Camera.h> is too generic a prefix to put on every consumer's include
# path. Every Systems target is now named after its own directory.
#
# Single-unit target, so include/ is flat and there is no sub-unit CMakeLists.
# Camera.h is the whole API: 16 call sites outside this target include it.
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
        src/Camera.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/Camera.h
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

# glm is PUBLIC: Camera.h's interface is glm matrices and vectors.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
)

# Camera.cpp uses MathUtils; Camera.h does not, so Utils stays PRIVATE.
target_link_libraries(${CLASS_NAME} PRIVATE
        Utils
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
