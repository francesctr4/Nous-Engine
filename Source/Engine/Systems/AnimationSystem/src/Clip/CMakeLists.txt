set(CLASS_NAME "Clip")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Clip/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        AnimChannel.cpp
        AnimInstance.cpp
)
