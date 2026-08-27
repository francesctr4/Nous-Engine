set(CLASS_NAME "Mesh")

# All three headers are API: ModuleScene spawns meshes through ImporterMesh.h,
# and ModuleResourceManager holds the SubMeshCache.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
        SubMeshCache.cpp
)
