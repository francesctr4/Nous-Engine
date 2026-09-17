set(CLASS_NAME "AnimationController")

# Both headers are API, for the same reason ResourceAudioGraph's are: the editor's
# AnimationControllerEditor calls WriteControllerToFile and CreateNewControllerFile
# directly, so Importer${CLASS_NAME}.h is public rather than private-by-includer-count.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
)
