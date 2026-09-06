set(CLASS_NAME "CBoneAttachment")

# Private half only: ${CLASS_NAME}.h is API and lives at
# include/ECS/Component/Types/${CLASS_NAME}/${CLASS_NAME}.h.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
)
