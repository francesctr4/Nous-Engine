# ===========================================
# NOUS_Job Class
# ===========================================

set(CLASS_NAME "NOUS_JobSystem")

target_sources(NOUS_Multithreading PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()