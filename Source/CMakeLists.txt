add_subdirectory(Editor)
add_subdirectory(Engine)
add_subdirectory(Game)

# EditorRuntime install rules — included after all subdirectories so every
# target (Nous-Engine, Nous-Editor, Scripts, EditorApp) is in scope.
include("${CMAKE_SOURCE_DIR}/cmake/Install/InstallEngine.cmake")