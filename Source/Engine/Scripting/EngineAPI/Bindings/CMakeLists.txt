target_sources(Scripting PRIVATE
        ScriptBindings.h
        ScriptBindings.cpp
)

add_subdirectory(GameObject)
add_subdirectory(Input)
add_subdirectory(Logger)