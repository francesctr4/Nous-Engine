set(CLASS_NAME "Skinning")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Skinning/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        Palette.cpp
        Bounds.cpp
)
