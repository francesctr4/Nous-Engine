set(CLASS_NAME "FreeList")

# Private half only: FreeList.h is API (VulkanBuffer.cpp uses it) and lives at
# include/MemoryManager/FreeList.h, listed in the target root's CMakeLists.
target_sources(MemoryManager PRIVATE
        ${CLASS_NAME}.cpp
)
