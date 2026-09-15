set(CLASS_NAME "Transform")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/(flat -- every unit includes it), listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        Transform.cpp
)
