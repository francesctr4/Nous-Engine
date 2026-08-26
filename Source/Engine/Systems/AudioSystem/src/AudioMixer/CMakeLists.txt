set(CLASS_NAME "AudioBusMath")

# Backend-agnostic bus policy (mute/solo math), deliberately private: its only
# consumer is MiniaudioBusGraph, and the editor talks to ModuleAudio instead.
target_sources(AudioSystem PRIVATE
        ${CLASS_NAME}.h
)
