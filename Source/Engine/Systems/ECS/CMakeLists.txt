# ==============================
# Nous Engine - ECS
# ==============================

add_library(ECS STATIC)

add_subdirectory(Scene)
add_subdirectory(GameObject)

target_sources(ECS PRIVATE
        Component/CTransform/ComponentTransform.h
        Component/CMesh/ComponentMesh.h
        Component/CMaterial/ComponentMaterial.h
        Component/CScript/ComponentScript.h
        Component/Component.h
        Component/Component.cpp
        Component/CTransform/ComponentTransform.cpp
        Component/CMesh/ComponentMesh.cpp
)

set_target_properties(ECS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ECS PUBLIC ../../..)

target_compile_definitions(ECS PRIVATE NOUS_ENGINE_EXPORTS)

find_package(glm REQUIRED)
target_link_libraries(ECS PUBLIC glm::glm)