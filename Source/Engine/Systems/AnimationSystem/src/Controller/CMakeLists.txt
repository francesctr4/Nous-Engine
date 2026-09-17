set(CLASS_NAME "Controller")

# Private half only: this unit's headers are API and live under
# include/AnimationSystem/Controller/, listed in the target root's CMakeLists.
target_sources(AnimationSystem PRIVATE
        Controller.cpp
        AnimParameters.cpp
)
