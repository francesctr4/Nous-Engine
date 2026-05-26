set(CLASS_NAME "AudioSystem")

add_library(${CLASS_NAME} STATIC
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

add_subdirectory(AudioEngine)