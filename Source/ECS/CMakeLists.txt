# ==============================
# Nous Engine - ECS
# ==============================

set(ECS_SOURCES
        GameObject.h
        Scene.h
        Components/ComponentTransform.h
        Components/ComponentMesh.h
        Components/ComponentMaterial.h
        Components/ComponentScript.h
        Component.h
        Component.cpp
        Components/ComponentTransform.cpp
        Components/ComponentMesh.cpp
)

add_library(ECS STATIC ${ECS_SOURCES})
add_library(NousEngine::ECS ALIAS ECS)

set_target_properties(ECS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ECS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

find_package(glm REQUIRED)
target_link_libraries(ECS PUBLIC glm::glm)