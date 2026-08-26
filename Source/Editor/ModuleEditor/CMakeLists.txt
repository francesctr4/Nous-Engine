set(CLASS_NAME "ModuleEditor")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_EDITOR_EXPORTS)

find_package(glm CONFIG REQUIRED)
target_link_libraries(${CLASS_NAME} PUBLIC
        glm::glm
        # ModuleEditor.cpp includes AudioGraphEditor.h, which names the effect
        # registry. Headers only.
        AudioSystem_headers
)
