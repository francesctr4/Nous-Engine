# GameExporter compiles into ModuleEditor, not a target of its own -- so its
# public header lives in ModuleEditor's include tree (<ModuleEditor/GameExporter.h>)
# while its source stays here. That is the same split EditorUI's window leaves use:
# one include tree per TARGET, sources wherever the unit lives.
target_sources(ModuleEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GameExporter.cpp
)

if(NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
