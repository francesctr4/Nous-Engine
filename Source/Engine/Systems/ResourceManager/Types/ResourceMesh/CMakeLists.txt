set(CLASS_NAME "Mesh")

target_sources(ResourceManager PRIVATE
        include/Importer${CLASS_NAME}.h
        include/Resource${CLASS_NAME}.h
        include/SubMeshCache.h
        src/Importer${CLASS_NAME}.cpp
        src/Resource${CLASS_NAME}.cpp
        src/SubMeshCache.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()