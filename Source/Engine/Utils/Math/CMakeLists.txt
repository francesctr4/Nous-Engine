target_sources(Utils PRIVATE
        MathUtils.h
        Vertex.inl
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()