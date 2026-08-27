set(CLASS_NAME "Scene")

# Private half only: Scene.h and iSceneHost.h are API and live under
# include/ECS/Scene/, listed in the target root's CMakeLists.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
)
