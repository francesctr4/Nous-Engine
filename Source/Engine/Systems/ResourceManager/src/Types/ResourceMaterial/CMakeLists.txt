set(CLASS_NAME "Material")

# Both headers are API: the inspector authors materials through
# ImporterMaterial.h.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
)
