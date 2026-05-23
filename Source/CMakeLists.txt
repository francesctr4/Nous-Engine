add_subdirectory(Editor)
add_subdirectory(Engine)
add_subdirectory(Game)

# GameApp must be built alongside EditorApp so the in-engine Build pipeline
# always has a GameApp.exe to copy into the game delivery.
add_dependencies(EditorApp GameApp)

# EditorRuntime install rules — included after all subdirectories so every
# target (Nous-Engine, Nous-Editor, Scripts, EditorApp) is in scope.
include("${CMAKE_SOURCE_DIR}/cmake/Install/InstallEngine.cmake")
