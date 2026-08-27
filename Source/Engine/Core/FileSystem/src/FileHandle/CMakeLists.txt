set(CLASS_NAME "FileHandle")

# Private half only: the public header is API and lives at
# include/FileSystem/FileHandle.h, listed in the target root's CMakeLists.
target_sources(FileSystem PRIVATE
        ${CLASS_NAME}.cpp
)
