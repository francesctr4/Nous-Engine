set(CLASS_NAME "CAudioSource")

# Private half only. Note AudioVoice.h is API too, not implementation: it is
# included by the public CAudioSource.h, which holds two AudioVoice members.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
        AudioVoice.cpp
)
