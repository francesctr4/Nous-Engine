set(CLASS_NAME "InspectorWindow")

target_sources(EditorUI PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)

add_subdirectory(InspectorComponent)
