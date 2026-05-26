set(CLASS_NAME "AudioGraphEditor")

target_sources(EditorUI PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
target_link_libraries(EditorUI PRIVATE
        unofficial::imgui-node-editor::imgui-node-editor
)