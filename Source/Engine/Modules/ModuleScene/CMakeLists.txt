set(CLASS_NAME "ModuleScene")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        glm::glm
        PrefabManager
        ModuleBase
        ECS
        EnTT::EnTT
)

target_link_libraries(${CLASS_NAME} PRIVATE
        CameraSystem
        EventSystem
        FileSystem
        Logger
        MemoryManager
        ModuleInput
        ModuleResourceManager
        NOUS_Multithreading
        ResourceManager
        SDL3::SDL3
        Scripting
        TimeManager
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
