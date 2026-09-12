target_sources(EditorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/AnimationControllerEditor.cpp
)

# The second window to declare a dependency of its own, for the same reason as
# AudioGraphEditor: the node editor is used by these two files alone. Declared here
# rather than in EditorUI's root block. find_package is idempotent, and
# target_link_libraries de-duplicates, so declaring it in both leaves is correct --
# neither may assume the other was configured first.
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
target_link_libraries(EditorUI PRIVATE
        unofficial::imgui-node-editor::imgui-node-editor
)

# The ARCHIVE, not just the headers, and this is the one place in the editor where
# that is correct. ControllerGraph::FindState is defined out of line in
# Controller.cpp, and AnimationSystem carries no NOUS_ENGINE_API -- so its symbols
# are not exported from the engine DLL and cannot be reached through it.
#
# Linking an engine archive ALONGSIDE the engine DLL is normally wrong: the consumer
# ends up with two copies of every engine global, which for most targets means two
# memory pools and two logger states. It is safe here precisely because
# AnimationSystem is the pure layer -- it links glm alone, holds no mutable global
# state, and is nothing but functions over data the caller passes in. A second copy
# of that is duplicated code and no duplicated state.
#
# If this target ever needs a symbol from a STATEFUL engine archive, this is not the
# pattern to copy: export it from the DLL instead.
target_link_libraries(EditorUI PRIVATE
        AnimationSystem
)
