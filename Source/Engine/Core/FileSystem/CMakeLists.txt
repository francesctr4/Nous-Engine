set(CLASS_NAME "FileSystem")

add_library(${CLASS_NAME} STATIC
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)

add_subdirectory(FileHandle)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(${CLASS_NAME} PRIVATE
        MemoryManager
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()