# ==============================
# Nous Engine - Systems
# ==============================

set(SYSTEMS_SOURCES
        "Camera System/Camera.cpp"
        "Camera System/Camera.h"

        "Event System/EventSystem.h"

        "File System/FileHandle.cpp"
        "File System/FileHandle.h"
        "File System/FileManager.cpp"
        "File System/FileManager.h"

        "Geometry System/GeometrySystem.cpp"
        "Geometry System/GeometrySystem.h"
        "Geometry System/Vertex.inl"

        "Material System/MaterialSystem.cpp"
        "Material System/MaterialSystem.h"

        "Memory Manager/MemoryManager.cpp"
        "Memory Manager/MemoryManager.h"

        "Memory Manager/Custom Allocators/DynamicAllocator.cpp"
        "Memory Manager/Custom Allocators/DynamicAllocator.h"
        "Memory Manager/Custom Allocators/FreeList.cpp"
        "Memory Manager/Custom Allocators/FreeList.h"
        "Memory Manager/Custom Allocators/LinearAllocator.cpp"
        "Memory Manager/Custom Allocators/LinearAllocator.h"

        "Resource Manager/MetaFileData.inl"

        "Resource Manager/Importers/Importer.inl"
        "Resource Manager/Importers/ImporterManager.cpp"
        "Resource Manager/Importers/ImporterManager.h"
        "Resource Manager/Importers/ImporterMaterial.cpp"
        "Resource Manager/Importers/ImporterMaterial.h"
        "Resource Manager/Importers/ImporterMesh.cpp"
        "Resource Manager/Importers/ImporterMesh.h"
        "Resource Manager/Importers/ImporterTexture.cpp"
        "Resource Manager/Importers/ImporterTexture.h"

        "Resource Manager/Resource Types/Resource.cpp"
        "Resource Manager/Resource Types/Resource.h"
        "Resource Manager/Resource Types/ResourceMaterial.cpp"
        "Resource Manager/Resource Types/ResourceMaterial.h"
        "Resource Manager/Resource Types/ResourceMesh.cpp"
        "Resource Manager/Resource Types/ResourceMesh.h"
        "Resource Manager/Resource Types/ResourceTexture.cpp"
        "Resource Manager/Resource Types/ResourceTexture.h"

        "Texture System/TextureSystem.cpp"
        "Texture System/TextureSystem.h"

        "Time Management/TimeManager.h"
        "Time Management/Timer.cpp"
        "Time Management/Timer.h"
)

add_library(Systems STATIC ${SYSTEMS_SOURCES})
add_library(NousEngine::Systems ALIAS Systems)

set_target_properties(Systems PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Systems PUBLIC ../..)

find_package(glm REQUIRED)
target_link_libraries(Systems PUBLIC glm::glm)