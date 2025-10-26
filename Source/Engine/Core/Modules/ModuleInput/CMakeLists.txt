set(CLASS_NAME "ModuleInput")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(${CLASS_NAME} PUBLIC SDL3::SDL3)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()