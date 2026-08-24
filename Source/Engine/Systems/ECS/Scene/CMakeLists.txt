set(CLASS_NAME "Scene")

target_sources(ECS PRIVATE
        include/${CLASS_NAME}.h
        include/iSceneHost.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()