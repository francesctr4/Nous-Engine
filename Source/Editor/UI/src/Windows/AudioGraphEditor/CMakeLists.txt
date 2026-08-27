target_sources(EditorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioGraphEditor.cpp
)

# This leaf is the ONLY window that declares a dependency of its own -- the node
# editor is used by AudioGraphEditor.cpp alone, so it is declared here rather than
# in EditorUI's root block, and stays PRIVATE (no public header names it).
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
target_link_libraries(EditorUI PRIVATE
        unofficial::imgui-node-editor::imgui-node-editor
)
