set(CLASS_NAME "Component")

# Private half only: Component.h, ComponentList.h and Types/ComponentTypes.h are
# all API and live under include/ECS/Component/.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
)

add_subdirectory(Types)
