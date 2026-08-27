set(CLASS_NAME "AudioGraph")

# Both headers are API: AudioGraphEditor calls WriteAudioGraphToFile and
# CreateNewAudioGraphFile directly, so ImporterAudioGraph.h is public.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
)
