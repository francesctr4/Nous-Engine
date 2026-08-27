set(CLASS_NAME "Scene")

# The only type with NO public header. ImporterScene is the pipeline-only
# importer from the IAssetImporter/IResourceImporter split -- it mirrors an asset
# into Library/ and produces no runtime Resource object, so there is no
# ResourceScene.h to expose.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.h
        Importer${CLASS_NAME}.cpp
)
