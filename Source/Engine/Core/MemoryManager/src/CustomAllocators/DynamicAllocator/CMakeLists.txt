set(CLASS_NAME "DynamicAllocator")

# Fully private: the header and the .cpp both live here, because nothing outside
# MemoryManager constructs one.
target_sources(MemoryManager PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)
