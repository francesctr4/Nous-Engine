# ===========================================
# NOUS_Job Class
# ===========================================

set(CLASS_NAME "NOUS_JobSystem")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()