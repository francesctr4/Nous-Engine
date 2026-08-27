set(CLASS_NAME "LinearAllocator")

# Fully private, same as DynamicAllocator.
target_sources(MemoryManager PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)
