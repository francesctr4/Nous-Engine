set(CLASS_NAME "ComponentInspectorRegistry")

target_sources(EditorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${CLASS_NAME}.h
        ${CMAKE_CURRENT_SOURCE_DIR}/${CLASS_NAME}.cpp
)

# The ARCHIVE, for the third time in this target and for the same reason recorded in
# full in Windows/AnimationControllerEditor/CMakeLists.txt: the Animator's live
# parameter panel calls AnimParameters' setters and getters, which are defined out of
# line in AnimParameters.cpp and carry no NOUS_ENGINE_API, so they are not reachable
# through the engine DLL. Safe only because AnimationSystem is the pure layer -- a
# second copy of stateless functions over caller-owned data. target_link_libraries
# de-duplicates, so declaring it here as well as in the two animation windows is
# correct: no leaf may assume another was configured first.
target_link_libraries(EditorUI PRIVATE
        AnimationSystem
)
