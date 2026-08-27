# ==============================
# Third-party Dependencies
# ==============================
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

# ==============================
# Engine Modules
# ==============================
add_subdirectory(Core)
add_subdirectory(Modules)
add_subdirectory(Scripting)
add_subdirectory(NOUS_Multithreading)
add_subdirectory(Utils)
add_subdirectory(Renderer)
add_subdirectory(Systems)

# ==============================
# NousEngine DLL (Shared Library)
# ==============================

add_library(${PROJECT_NAME} SHARED EngineExport.h MainEngine.cpp)
add_library(NousEngine::Engine ALIAS ${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Include directories
target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Link engine modules + dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
        # glm appears in public headers (RendererTypes.h, etc.) so must be PUBLIC
        glm::glm

        # ---- Converted-target header dirs --------------------------------------
        # Everything else is linked PRIVATE below, so the DLL propagates nothing
        # and downstream targets (EditorApp, GameApp, the component tests) have
        # been resolving engine headers purely through the global Source/ include
        # root. As each target moves to the include/<Target>/ layout, its
        # header-only INTERFACE handle is added here — that is what keeps the DLL's
        # own public API (ModuleAudio.h, CAudioSource.h, ... all name AudioSystem
        # types) resolvable once the global root goes away.
        #
        # Add one line per converted target. When the list is complete, the global
        # Source/ root can be dropped and enforcement becomes engine-wide.
        AudioSystem_headers
        FileSystem_headers
        Logger_headers
        MemoryManager_headers
        NOUS_Multithreading_headers
        CameraSystem_headers
        ShaderSystem_headers
        VideoSystem_headers
        ECS_headers
        ResourceManager_headers
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        # --- Third-Party ---
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        parson::parson
        Tracy::TracyClient
)

# Force all symbols from static engine modules into the shared library.
# Without this, only symbols directly referenced by MainEngine.cpp are exported,
# causing undefined reference errors at link time for downstream targets.
#
# We use target_link_options with $<TARGET_FILE:...> for the whole-archive flags
# on all platforms. This avoids CMake's library deduplication, which would otherwise
# move sub-libraries out of the whole-archive range into the normal link section.
# target_link_libraries is still used for CMake dependency tracking.

# The list is DERIVED, never hand-written. It used to be a manually maintained
# copy of the target graph and had silently drifted five targets out of date
# (ModuleAudio, ModuleVideo, AudioSystem, VideoSystem, PrefabManager). Those kept
# working only because Application.cpp happens to reference them directly, so the
# linker pulled their objects in anyway. A target reached solely through a
# registration table, a factory, or virtual dispatch would have failed silently,
# either as an LNK2019 somewhere unrelated or as a registration that never runs.
#
# CMake already knows every target, so ask it. Walking BUILDSYSTEM_TARGETS makes
# the list correct by construction: a new leaf library is picked up the moment its
# add_subdirectory runs, with nothing to remember.
#
# Filtering on TYPE is load-bearing, not defensive: it drops the ~63 test
# executables and, critically, the `Scripts` SHARED library, which is the
# hot-reloaded script plugin and must stay a separate DLL.
#
# Order is alphabetical for reproducibility. Safe because whole-archive pulls in
# every object from every listed archive, so there is no archive-order symbol
# resolution to depend on (which is also why the old aggregates-first ordering
# was never load-bearing).
function(nous_collect_static_libs DIR OUT_VAR)
    set(collected "")

    get_property(dirTargets DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS dirTargets)
        get_target_property(targetType ${target} TYPE)
        if (targetType STREQUAL "STATIC_LIBRARY")
            list(APPEND collected ${target})
        endif()
    endforeach()

    get_property(subDirs DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
    foreach(subDir IN LISTS subDirs)
        nous_collect_static_libs("${subDir}" subCollected)
        list(APPEND collected ${subCollected})
    endforeach()

    set(${OUT_VAR} "${collected}" PARENT_SCOPE)
endfunction()

# Must run after every add_subdirectory above — a directory not yet processed
# reports no targets.
nous_collect_static_libs("${CMAKE_CURRENT_SOURCE_DIR}" NOUS_ENGINE_STATIC_LIBS)
list(REMOVE_DUPLICATES NOUS_ENGINE_STATIC_LIBS)
list(SORT NOUS_ENGINE_STATIC_LIBS)

if (NOT NOUS_ENGINE_STATIC_LIBS)
    message(FATAL_ERROR
        "No static engine libraries were collected. Every engine symbol would be "
        "dropped from the DLL and downstream targets would fail with LNK2019. "
        "Check that the add_subdirectory() calls above run before this point.")
endif()

list(LENGTH NOUS_ENGINE_STATIC_LIBS NOUS_ENGINE_STATIC_LIB_COUNT)
message(STATUS
    "NousEngine: whole-archiving ${NOUS_ENGINE_STATIC_LIB_COUNT} static libraries "
    "into the engine DLL")

if (WIN32)
  foreach(lib IN LISTS NOUS_ENGINE_STATIC_LIBS)
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:${lib}>")
  endforeach()
elseif(APPLE)
  foreach(lib IN LISTS NOUS_ENGINE_STATIC_LIBS)
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-force_load,$<TARGET_FILE:${lib}>")
  endforeach()
else()
  # Linux (GNU ld): per-library --whole-archive via LINKER: prefix.
  # "LINKER:--whole-archive,path,--no-whole-archive" expands to:
  #   -Wl,--whole-archive,/abs/path/to/lib.a,--no-whole-archive
  # which passes three separate args to ld, scoping --whole-archive to just that archive.
  foreach(lib IN LISTS NOUS_ENGINE_STATIC_LIBS)
    target_link_options(${PROJECT_NAME} PRIVATE
      "LINKER:--whole-archive,$<TARGET_FILE:${lib}>,--no-whole-archive")
  endforeach()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${NOUS_ENGINE_STATIC_LIBS})

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Engine"
)