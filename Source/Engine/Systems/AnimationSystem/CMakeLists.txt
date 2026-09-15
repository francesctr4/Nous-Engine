set(CLASS_NAME "AnimationSystem")

# ==============================================================================
# AnimationSystem -- the PURE animation library.
#
#   include/AnimationSystem/**  is the API, reached as <AnimationSystem/Pose.h>.
#   src/**                      is implementation.
#
# Dependency policy, and it is the point of this target: glm and the standard
# library, nothing else. No ECS, no Modules, no ResourceManager, no Logger, no
# MemoryManager. That is what lets the four t_AnimationSystem_* tests link this
# archive plus gtest and NOTHING ELSE -- the same property AudioSystem's GainDsp
# and AudioBusMath have, and the reason the spec's build order can prove every
# piece of maths before any importer or module exists.
#
# The layers above compose these types rather than inherit engine knowledge into
# them: ResourceSkeleton will hold a SkeletonData by value, ResourceAnimation an
# AnimClipData. If a future change here wants Logger or ResourceManager, that is
# the signal the code belongs in CAnimator, an importer, or a future animation
# module instead.
#
# No NOUS_ENGINE_API markers: nothing in Source/Editor or Source/Game names these
# types today. Add them (and only them) if the editor ever needs a pose directly.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# glm is PUBLIC on the headers handle: Transform, Pose and Palette all express
# their interface in glm types, so a consumer of these headers needs glm too.
target_link_libraries(${CLASS_NAME}_headers INTERFACE
        EngineCore_headers
        glm::glm
)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC)

# Grouped by PIPELINE STAGE, not by file name, and the two judgement calls are
# worth knowing: Skinning/ holds Palette AND Bounds because both exist to feed the
# GPU rather than to sample (Bounds is per-bone SKINNED bounds, built from the
# palette); Controller/ holds AnimParameters because the graph is its only reader
# -- the scripting API writes them, but it writes them FOR transitions.
#
# Transform.h and Pose.h stay FLAT at the target root: every other unit includes
# them, so a folder would be a hop with nothing behind it. Same call AudioTypes.h
# and VideoFrame.h make in their targets. Their sources still get a src/ leaf, per
# the layout rule in CONVENTIONS.md.
target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/Transform.h
        include/${CLASS_NAME}/Pose.h

        include/${CLASS_NAME}/Clip/AnimChannel.h
        include/${CLASS_NAME}/Clip/AnimClip.h
        include/${CLASS_NAME}/Clip/AnimInstance.h

        include/${CLASS_NAME}/Skeleton/Skeleton.h

        include/${CLASS_NAME}/Sampling/Binding.h
        include/${CLASS_NAME}/Sampling/Sampling.h
        include/${CLASS_NAME}/Sampling/Blending.h

        include/${CLASS_NAME}/Skinning/Palette.h
        include/${CLASS_NAME}/Skinning/Bounds.h

        include/${CLASS_NAME}/Controller/Controller.h
        include/${CLASS_NAME}/Controller/AnimParameters.h

        include/${CLASS_NAME}/RootMotion/RootMotion.h

        include/${CLASS_NAME}/Events/AnimationEvents.h
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

# One per unit, each adding its own .cpp files through target_sources. The leaf
# CMakeLists is the unit boundary marker the rest of the tree uses -- it is why a
# new unit is a directory rather than another line in a list here.
add_subdirectory(src/Transform)
add_subdirectory(src/Clip)
add_subdirectory(src/Skeleton)
add_subdirectory(src/Sampling)
add_subdirectory(src/Skinning)
add_subdirectory(src/Controller)
add_subdirectory(src/RootMotion)
add_subdirectory(src/Events)

# Deliberately no PRIVATE link line. Logger, MemoryManager and ResourceManager
# were declared here while this target was an empty skeleton; none of them is
# used, and ResourceManager in particular would have wired this library into the
# AudioSystem <-> ResourceManager <-> VideoSystem cycle cluster for nothing.

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
