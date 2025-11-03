# ===========================================
# NOUS_Job Class
# ===========================================

set(CLASS_NAME "NOUS_ThreadPool")

target_sources(NOUS_Multithreading PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()