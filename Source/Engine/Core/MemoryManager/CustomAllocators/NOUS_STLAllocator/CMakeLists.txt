set(CLASS_NAME "NOUS_STLAllocator")

target_sources(MemoryManager PRIVATE
        include/${CLASS_NAME}.h
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()