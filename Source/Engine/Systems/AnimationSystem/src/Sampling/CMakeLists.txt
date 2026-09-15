set(CLASS_NAME "Sampling")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Sampling/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        Binding.cpp
        Sampling.cpp
        Blending.cpp
)
