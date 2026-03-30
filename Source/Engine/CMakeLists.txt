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

set(NOUS_ENGINE_STATIC_LIBS
    # Top-level aggregates
    Core Modules NOUS_Multithreading Renderer Scripting Systems Utils
    # Core sub-libraries
    MemoryManager EventSystem FileSystem FileWatcher Logger TimeManager
    # Modules sub-libraries
    ModuleCamera3D ModuleInput ModuleRenderer3D ModuleResourceManager ModuleScene ModuleWindow
    # Renderer sub-libraries
    RendererFrontend RendererBackend VulkanBackend
    # Systems sub-libraries
    Camera ECS ResourceManager ShaderSystem
)

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