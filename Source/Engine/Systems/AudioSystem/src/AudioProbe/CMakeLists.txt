set(CLASS_NAME "AudioProbe")

# Implementation only; AudioProbe.h is API (ImporterAudio calls it) and lives
# under include/AudioSystem/AudioProbe/.
target_sources(AudioSystem PRIVATE
        ${CLASS_NAME}.cpp
)
