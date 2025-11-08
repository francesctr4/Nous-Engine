# ==============================
# Nous Engine - Systems
# ==============================

add_subdirectory(CameraSystem)
add_subdirectory(ECS)

set(SYSTEMS_SOURCES
        "ResourceManager/MetaFileData.inl"

        "ResourceManager/Importers/Importer.inl"
        "ResourceManager/Importers/ImporterManager.cpp"
        "ResourceManager/Importers/ImporterManager.h"
        "ResourceManager/Importers/ImporterMaterial.cpp"
        "ResourceManager/Importers/ImporterMaterial.h"
        "ResourceManager/Importers/ImporterMesh.cpp"
        "ResourceManager/Importers/ImporterMesh.h"
        "ResourceManager/Importers/ImporterTexture.cpp"
        "ResourceManager/Importers/ImporterTexture.h"

        "ResourceManager/ResourceTypes/Resource.cpp"
        "ResourceManager/ResourceTypes/Resource.h"
        "ResourceManager/ResourceTypes/ResourceMaterial.cpp"
        "ResourceManager/ResourceTypes/ResourceMaterial.h"
        "ResourceManager/ResourceTypes/ResourceMesh.cpp"
        "ResourceManager/ResourceTypes/ResourceMesh.h"
        "ResourceManager/ResourceTypes/ResourceTexture.cpp"
        "ResourceManager/ResourceTypes/ResourceTexture.h"
)

add_library(Systems STATIC
        ${SYSTEMS_SOURCES}
)

set_target_properties(Systems PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Systems PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Systems PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(Systems PUBLIC
        glm::glm
        Camera
        ECS
)