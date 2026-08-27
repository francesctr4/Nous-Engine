# ==============================================================================
# EditorCore — the bottom of the editor graph.
#
#   include/EditorCore/**  is the API, reached as <EditorCore/EditorContext.h>.
#
# EditorExport.h and EditorContext.h used to sit at Editor/ root and belong to
# the Nous-Editor SHARED target. That was upside down: Nous-Editor LINKS EditorUI
# and ModuleEditor, and both of those include these two headers. It resolved only
# because the global Source/ include root is on every target's path; declared
# honestly it is a cycle, and CMake REJECTS cycles between INTERFACE libraries
# (unlike the STATIC-library cycles this tree relies on elsewhere).
#
# So the base is split from the aggregate, exactly as ModuleBase was split out of
# Modules: EditorCore sits at the BOTTOM (everyone links it), Nous-Editor stays
# at the TOP (it links everyone). EditorContext is the editor's service locator,
# so the bottom of the graph is where it already belonged.
#
# Header-only: there is no EditorContext.cpp, so this target compiles nothing and
# is INTERFACE all the way down.
# ==============================================================================

add_library(EditorCore_headers INTERFACE)
target_include_directories(EditorCore_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(EditorCore INTERFACE)
target_link_libraries(EditorCore INTERFACE EditorCore_headers)
