set(CLASS_NAME "CTransform")

# Private half only: ${CLASS_NAME}.h is API and lives at
# include/ECS/Component/Types/${CLASS_NAME}.h.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
)
