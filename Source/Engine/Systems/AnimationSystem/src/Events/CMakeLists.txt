set(CLASS_NAME "Events")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Events/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        AnimationEvents.cpp
)
