set(CLASS_NAME "HotReloader")

# Private half only: this unit's header is API and lives under
# include/ResourceManager/Runtime/.
target_sources(ResourceManager PRIVATE
        HotReloader.cpp
)
