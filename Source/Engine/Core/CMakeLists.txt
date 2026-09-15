# ==============================
# Nous Engine - Core
# ==============================
#
#   include/Core/Application.h  is the API — the composition root, named only by
#                               MainEditor.cpp, MainGame.cpp and its own .cpp.
#   src/**                      is implementation.
#
# Globals.h, UpdateStatus.h and IInputReader.h used to live here. They are at the
# BOTTOM of the include graph while Application is at the TOP, so they moved to
# the new header-only EngineCore target; see Source/Engine/EngineCore/CMakeLists.txt.

add_subdirectory(CrashHandler)
add_subdirectory(EventSystem)
add_subdirectory(FileSystem)
add_subdirectory(FileWatcher)
add_subdirectory(Logger)
add_subdirectory(MemoryManager)
add_subdirectory(TimeManager)

add_library(Core_headers INTERFACE)
target_include_directories(Core_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(Core_headers INTERFACE EngineCore_headers)

add_library(Core STATIC
        src/Application.cpp
        include/Core/Application.h
)

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Core
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Core PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Core PUBLIC
        Core_headers
        EventSystem
        FileSystem
        FileWatcher
        TimeManager
        Logger
        MemoryManager
)

# Application is the composition root: it constructs every module and the
# ImporterManager/TypeRegistry, so Core depends UPWARD on Modules here. That is
# correct for a composition root, but it does mean "Core is the bottom layer" is
# false -- the foundation is EngineCore plus EventSystem/FileSystem/Logger/
# MemoryManager/TimeManager above, not this target. Nothing depends back on
# Application, so these edges are one-way and the graph stays acyclic.
target_link_libraries(Core PRIVATE
        Modules
        NOUS_Multithreading
        ResourceManager
        Scripting
        SDL3::SDL3
)

# Application.cpp includes <tracy/Tracy.hpp> under _PROFILING. The vcpkg tracy
# port installs its headers at include/tracy/tracy/Tracy.hpp, so that spelling
# resolves ONLY through the include dir Tracy::TracyClient carries -- it is not
# reachable from the vcpkg_installed/include root.
if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(Core PRIVATE Tracy::TracyClient)
endif()
