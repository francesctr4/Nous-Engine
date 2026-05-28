target_sources(ResourceManager PRIVATE
        IResourceLoader.h
)

add_subdirectory(ImporterManager)
add_subdirectory(Resource)
add_subdirectory(TypeRegistry)