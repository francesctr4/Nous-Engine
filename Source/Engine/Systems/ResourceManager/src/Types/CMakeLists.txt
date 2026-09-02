# RegisterResourceTypes.cpp is the single TU that names every type-specific
# importer, which is what lets ImporterAudio/Scene/Shader/Texture/Video stay
# private. Keep it that way: reaching for one of those headers from anywhere
# else will not compile, and that is the intended answer.
target_sources(ResourceManager PRIVATE
        RegisterResourceTypes.cpp
)

add_subdirectory(ResourceAudio)
add_subdirectory(ResourceAudioGraph)
add_subdirectory(ResourceMaterial)
add_subdirectory(ResourceMesh)
add_subdirectory(ResourceScene)
add_subdirectory(ResourceShader)
add_subdirectory(ResourceSkeleton)
add_subdirectory(ResourceTexture)
add_subdirectory(ResourceVideo)
