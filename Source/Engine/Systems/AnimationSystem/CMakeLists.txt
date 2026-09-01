set(CLASS_NAME "AnimationSystem")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Flat src/ for now: the four types are still empty skeletons. Split into
# per-unit src/<Type>/ leaves with their own CMakeLists (as AudioSystem does)
# once any of them grows real implementation.
add_library(${CLASS_NAME} STATIC
        src/Bone.cpp
        src/ResourceAnimationClip.cpp
        src/ResourceSkeleton.cpp
        src/ResourceSkinnedMesh.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/Bone.h
        include/${CLASS_NAME}/ResourceAnimationClip.h
        include/${CLASS_NAME}/ResourceSkeleton.h
        include/${CLASS_NAME}/ResourceSkinnedMesh.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

# ResourceManager is PRIVATE only while the Resource* headers here are empty.
# The moment ResourceSkeleton/ResourceAnimationClip/ResourceSkinnedMesh derive
# ResourceBase in their PUBLIC headers, ResourceManager_headers must move to the
# PUBLIC list -- a consumer of those headers needs the dir too.
target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
        ResourceManager
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
