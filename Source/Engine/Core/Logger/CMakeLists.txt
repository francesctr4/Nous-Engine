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