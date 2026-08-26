set(CLASS_NAME "Logger")

add_library(${CLASS_NAME} STATIC
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
        Asserts.h
        LogChannel.h
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# NOUS_Multithreading is PUBLIC: Logger's async flush thread is part of its header
# surface. Note this closes a cycle -- FileSystem and MemoryManager both assert
# through Logger, and NOUS_Multithreading allocates through MemoryManager. CMake
# permits cycles between STATIC libraries; see the note in Source/Engine/CMakeLists.txt.
target_link_libraries(${CLASS_NAME} PUBLIC
        NOUS_Multithreading
)

target_link_libraries(${CLASS_NAME} PRIVATE
        FileSystem
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()