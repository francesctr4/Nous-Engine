# ==============================
# Nous Engine - Systems
# ==============================

add_subdirectory(ECS)

set(SYSTEMS_SOURCES
        "Camera System/Camera.cpp"
        "Camera System/Camera.h"

        "Event System/EventSystem.h"

        "File System/FileHandle.cpp"
        "File System/FileHandle.h"
        "File System/FileManager.cpp"
        "File System/FileManager.h"

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

        "Time Management/TimeManager.h"
        "Time Management/Timer.cpp"
        "Time Management/Timer.h"
        "Event System/IEventListener.h"
        "Event System/Event.h"
        "Event System/EventSystem.cpp"
        "Event System/Event.cpp"

        "Logging System/Asserts.h"
        "Logging System/Logger.cpp"
        "Logging System/Logger.h"
        "Logging System/LogChannel.h"
)

add_library(Systems STATIC ${SYSTEMS_SOURCES})

set_target_properties(Systems PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Systems PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Systems PRIVATE NOUS_ENGINE_EXPORTS)

find_package(glm REQUIRED)  # 👈 must come before add_library
target_link_libraries(Systems PUBLIC
        glm::glm
        ECS
)