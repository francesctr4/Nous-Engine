target_sources(EditorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/AnimationTimelineWindow.cpp
)

# The ARCHIVE, for the same reason AnimationControllerEditor links it, and under the
# same justification recorded there: AnimationSystem carries no NOUS_ENGINE_API, so
# its symbols are not reachable through the engine DLL. Safe ONLY because that target
# is the pure layer -- glm alone, no mutable global state -- so a second copy is
# duplicated code and no duplicated state. target_link_libraries de-duplicates, so
# declaring it in both leaves is correct; neither may assume the other configured first.
target_link_libraries(EditorUI PRIVATE
        AnimationSystem
)
