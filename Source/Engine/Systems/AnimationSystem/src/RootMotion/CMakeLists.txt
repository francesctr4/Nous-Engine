set(CLASS_NAME "RootMotion")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/RootMotion/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        RootMotion.cpp
)
