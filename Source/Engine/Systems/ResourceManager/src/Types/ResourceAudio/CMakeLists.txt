set(CLASS_NAME "Audio")

# Importer${CLASS_NAME}.h is PRIVATE and lives here: only RegisterResourceTypes.cpp
# and this .cpp name it. Resource${CLASS_NAME}.h is API, under
# include/ResourceManager/Types/Resource${CLASS_NAME}/.
target_sources(ResourceManager PRIVATE
        Importer${CLASS_NAME}.h
        Importer${CLASS_NAME}.cpp
        Resource${CLASS_NAME}.cpp
)
