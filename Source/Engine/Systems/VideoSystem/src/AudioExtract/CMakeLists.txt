set(CLASS_NAME "AudioExtract")

# Private half only: AudioExtract.h is API (ImportPipeline, ImporterVideo and the
# editor's inspector registry call it) and lives at include/VideoSystem/AudioExtract/.
target_sources(VideoSystem PRIVATE
        ${CLASS_NAME}.cpp
)
