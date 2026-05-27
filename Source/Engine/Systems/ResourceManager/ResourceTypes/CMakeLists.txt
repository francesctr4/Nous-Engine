target_sources(ResourceManager PRIVATE
        ResourceType.h
        RegisterBuiltinResourceTypes.cpp
)

add_subdirectory(Importer)
add_subdirectory(Resource)