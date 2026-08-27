set(CLASS_NAME "GameObject")

# Private half only: GameObject.h is API and lives at include/ECS/GameObject.h.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
)
