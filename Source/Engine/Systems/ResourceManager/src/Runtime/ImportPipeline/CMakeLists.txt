set(CLASS_NAME "ImportPipeline")

# Private half only: this unit's header is API and lives under
# include/ResourceManager/Runtime/.
target_sources(ResourceManager PRIVATE
        ImportPipeline.cpp
)
