# ==============================================================================
# EngineCore — the bottom of the engine graph.
#
#   include/EngineCore/**  is the API, reached as <EngineCore/Globals.h>.
#
# These four headers are the ones literally everything names: EngineExport.h and
# Globals.h have ~74 includers each, and both are reached THROUGH the public
# headers of already-converted targets (Logger/Logger.h, MemoryManager/MemoryManager.h,
# FileSystem/FileHandle.h, Scripting/iScriptInput.h, ModuleBase/Module.h, ...).
# They sit at the very bottom of the include graph.
#
# Yet EngineExport.h belonged to the NousEngine SHARED target and Globals.h /
# UpdateStatus.h / IInputReader.h to Core — the two targets at the TOP. That is
# upside down in exactly the way EditorExport.h/EditorContext.h were, and it only
# worked because the global Source/ include root is on every target's path.
# Declared honestly it is a cycle, and CMake REJECTS cycles between INTERFACE
# libraries (unlike the STATIC-library cycles this tree relies on elsewhere).
#
# So the base is split from the aggregate for the third time, after ModuleBase
# out of Modules and EditorCore out of Nous-Editor: EngineCore sits at the BOTTOM
# (everyone links it), Core keeps only Application.{h,cpp} and stays at the TOP
# (the composition root, which links everyone).
#
# The check that makes this safe: none of these four headers names anything but
# the C++ standard library, so EngineCore_headers carries NO link dependencies
# and introduces no INTERFACE cycle even though ~30 targets link it.
#
# Header-only: nothing here compiles, so the target is INTERFACE all the way down.
#
# NOTE: not to be confused with the runtime <bin>/EngineSDK/ directory, which
# holds the script SDK + build tooling. It was named EngineCore until this target
# claimed the name.
# ==============================================================================

add_library(EngineCore_headers INTERFACE)
target_include_directories(EngineCore_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(EngineCore INTERFACE)
target_link_libraries(EngineCore INTERFACE EngineCore_headers)
