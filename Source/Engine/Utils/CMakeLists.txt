# ==============================
# Nous Engine - Utils
# ==============================

add_library(Utils STATIC)

add_subdirectory(DataStructures)
add_subdirectory(Math)
add_subdirectory(Serialization)

set_target_properties(Utils PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Utils PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Utils PRIVATE
        NOUS_ENGINE_EXPORTS
)

find_package(parson CONFIG REQUIRED)

target_link_libraries(Utils PUBLIC
        glm::glm
        parson::parson
        # NOUS_Vector.h is a PUBLIC Utils header and routes STL containers through
        # NOUS_STLAllocator, so consumers need MemoryManager's API dir too.
        # Headers only -- no objects, no transitive deps.
        MemoryManager_headers
)

target_link_libraries(Utils PRIVATE
        FileSystem
        Logger
)