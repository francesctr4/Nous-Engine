# ==============================
# Nous Engine - Renderer Frontend
#
# include/RendererFrontend/**  is the API. src/** is implementation.
#
# All four headers are public, and GroupGeometries.h / DynamicTextureCache.h are
# the interesting pair: no consumer outside this target includes either DIRECTLY,
# so a raw includer count says "private". They are wrong to move, because
# RendererFrontend.h -- the public header -- includes both by value
# (mGroupedGeometries, mDynamicTextures are members, not pointers). A header
# reached THROUGH a public header is public, whatever the count says. Making them
# genuinely private means giving RendererFrontend.h a pimpl or forward-declared
# members first; that is a real refactor, not a file move.
# ==============================

set(CLASS_NAME "RendererFrontend")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/GroupGeometries.cpp
        src/DynamicTextureCache.cpp
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
        include/${CLASS_NAME}/IEditorOverlay.h
        include/${CLASS_NAME}/GroupGeometries.h
        include/${CLASS_NAME}/DynamicTextureCache.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
        Renderer
        # RendererFrontend.h names ShaderLoaderTypes in its PUBLIC header, so the
        # API dir has to propagate. The ShaderSystem objects stay PRIVATE below --
        # this is headers only. Mis-scoped until the include/ split made it fail
        # to compile in ModuleEditor.
        ShaderSystem_headers
        # RendererFrontend.h holds a vector<Vertex3D> by value for the debug-line
        # overlay, and RendererTypes.h only forward-declares Vertex3D -- so the API
        # dir has to propagate. Headers only.
        Utils_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        CameraSystem
        Logger
        MemoryManager
        NOUS_Multithreading
        RendererBackend   # CreateRendererBackend -- the compile firewall
        ResourceManager
        ShaderSystem
)

# RendererFrontend.cpp includes <tracy/Tracy.hpp> under _PROFILING. The vcpkg tracy
# port installs its headers at include/tracy/tracy/Tracy.hpp, so that spelling
# resolves ONLY through the include dir Tracy::TracyClient carries -- it is not
# reachable from the vcpkg_installed/include root.
if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${CLASS_NAME} PRIVATE Tracy::TracyClient)
endif()

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
