set(CLASS_NAME "ResourceBase")

# Private half only: this unit's headers are API and live under
# include/ResourceManager/Core/.
target_sources(ResourceManager PRIVATE
        ${CLASS_NAME}.cpp
)
