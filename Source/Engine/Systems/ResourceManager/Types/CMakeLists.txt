target_sources(ResourceManager PRIVATE
        ResourceType.h
        RegisterResourceTypes.cpp
)

add_subdirectory(ResourceAudio)
add_subdirectory(ResourceMaterial)
add_subdirectory(ResourceMesh)
add_subdirectory(ResourceScene)
add_subdirectory(ResourceShader)
add_subdirectory(ResourceTexture)
add_subdirectory(ResourceVideo)