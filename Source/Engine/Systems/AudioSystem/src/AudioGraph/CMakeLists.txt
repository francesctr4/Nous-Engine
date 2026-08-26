set(CLASS_NAME "AudioGraph")

# Private half only: the dependency-free DSP math. The public effect recipe +
# schema registry (AudioEffectTypes.h, AudioEffectRegistry.h) are API and live
# under include/AudioSystem/AudioGraph/.
target_sources(AudioSystem PRIVATE
        GainDsp.h
)
