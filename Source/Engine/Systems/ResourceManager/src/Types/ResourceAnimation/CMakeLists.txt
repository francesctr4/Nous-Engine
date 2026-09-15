set(CLASS_NAME "Animation")

# Both headers are API. ImporterAnimation.h in particular is public because
# ModelImport calls SaveClip directly -- the whole point of the model pre-pass is
# that one parse feeds every importer, so that entry point has to be reachable.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
)
