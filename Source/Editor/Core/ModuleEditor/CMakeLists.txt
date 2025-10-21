# ===========================================
# ModuleEditor Submodule
# ===========================================

# Object library — compiled objects are linked into Nous-Editor
add_library(ModuleEditor OBJECT
        include/ModuleEditor.h
        src/ModuleEditor.cpp
)

# Dependencies
find_package(glm REQUIRED)

target_link_libraries(ModuleEditor PUBLIC
        NousEngine::Engine
        glm::glm
)

target_include_directories(ModuleEditor PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(ModuleEditor PRIVATE NOUS_EDITOR_EXPORTS)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()