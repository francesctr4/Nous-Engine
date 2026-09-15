set(CLASS_NAME "Skeleton")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Skeleton/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        Skeleton.cpp
)
