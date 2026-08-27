target_sources(Scripting PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ScriptBindings.cpp
)

add_subdirectory(Camera)
add_subdirectory(Component)
add_subdirectory(GameObject)
add_subdirectory(Input)
add_subdirectory(Light)
add_subdirectory(Logger)
add_subdirectory(Material)
add_subdirectory(Scene)
add_subdirectory(Time)
