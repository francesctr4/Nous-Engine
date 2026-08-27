set(CLASS_NAME "ComponentInspectorRegistry")

target_sources(EditorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${CLASS_NAME}.h
        ${CMAKE_CURRENT_SOURCE_DIR}/${CLASS_NAME}.cpp
)
