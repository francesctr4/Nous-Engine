target_sources(ResourceManager PRIVATE
        MetaFileData.inl
        Resource.h
        Resource.cpp
)

add_subdirectory(ResourceMaterial)
add_subdirectory(ResourceMesh)
add_subdirectory(ResourceTexture)
add_subdirectory(ResourceShader)