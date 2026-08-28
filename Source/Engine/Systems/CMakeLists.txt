# ==============================
# Nous Engine - Systems
# ==============================

add_subdirectory(CameraSystem)
add_subdirectory(ECS)
add_subdirectory(PrefabManager)
add_subdirectory(ResourceManager)
add_subdirectory(ShaderSystem)
add_subdirectory(AudioSystem)
add_subdirectory(VideoSystem)

# SystemManager is genuinely private: its only includer is its own .cpp, so this
# target publishes no headers at all and gets no include/ dir.
add_library(Systems STATIC
        src/SystemManager.cpp
        src/SystemManager.h
)

set_target_properties(Systems PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Systems
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, removed with the last conversion
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Systems PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(Systems PUBLIC
        CameraSystem
        ECS
        PrefabManager
        ResourceManager
        ShaderSystem
        AudioSystem
        VideoSystem
)

target_link_libraries(Systems PRIVATE Logger)