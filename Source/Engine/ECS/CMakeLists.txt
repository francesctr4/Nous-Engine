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

add_library(ECS OBJECT ${ECS_SOURCES})

set_target_properties(ECS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ECS PUBLIC ../..)

find_package(glm REQUIRED)
target_link_libraries(ECS PUBLIC glm::glm)