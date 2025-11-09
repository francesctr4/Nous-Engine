target_sources(ResourceManager PRIVATE
        Importer.inl
        ImporterManager.h
        ImporterManager.cpp
)

add_subdirectory(ImporterMaterial)
add_subdirectory(ImporterMesh)
add_subdirectory(ImporterTexture)