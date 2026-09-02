# ==============================
# Nous Engine - ResourceManager
#
# Public/private header layout (see CONVENTIONS.md).
#
#   include/ResourceManager/**  is the API, e.g. <ResourceManager/Core/ResourceBase.h>.
#   src/**                      is implementation, unreachable from outside.
#
# What the split bought here: the five type-specific importers are now private.
# ImporterAudio/Scene/Shader/Texture/Video are named ONLY by their own .cpp and by
# RegisterResourceTypes.cpp, which is exactly right -- an importer is wiring,
# reached through TypeRegistry's createFn/destroyFn, and nothing outside this
# target should ever name one. ImporterMaterial, ImporterMesh and
# ImporterAudioGraph stay public because the editor genuinely calls them
# (material authoring, mesh spawning, .nafx save/new).
#
# ResourceScene has no public header at all: ImporterScene is the pipeline-only
# importer from the IAssetImporter/IResourceImporter split, with no runtime
# Resource type. The layout makes that visible rather than merely documented.
#
# include/ layout is mirrored per-unit under Types/ but flat under Core/ and
# Runtime/, and that asymmetry is deliberate. Adding a resource type is a
# recurring workflow, so a type's files stay one identically-named folder in each
# of include/, src/ and test/ -- the add-a-type checklist is "the three
# ResourceX folders". Core/ and Runtime/ units are one-off; a folder per unit
# there would only produce HotReloader/HotReloader.h. Do not "tidy" this into
# symmetry.
# ==============================

set(CLASS_NAME "ResourceManager")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/Core/AssetPaths.h
        include/${CLASS_NAME}/Core/IImporter.h
        include/${CLASS_NAME}/Core/IImporterDispatcher.h
        include/${CLASS_NAME}/Core/IImporterManager.h
        include/${CLASS_NAME}/Core/ImporterManager.h
        include/${CLASS_NAME}/Core/IResourceLoader.h
        include/${CLASS_NAME}/Core/IResourceGpuSync.h
        include/${CLASS_NAME}/Core/MetaFileData.h
        include/${CLASS_NAME}/Core/ResourceBase.h
        include/${CLASS_NAME}/Core/ResourceQueue.h
        include/${CLASS_NAME}/Core/ResourceUpload.h
        include/${CLASS_NAME}/Core/ResourceTable.h
        include/${CLASS_NAME}/Core/TypeRegistry.h

        include/${CLASS_NAME}/Runtime/BuiltinResources.h
        include/${CLASS_NAME}/Runtime/HotReloader.h
        include/${CLASS_NAME}/Runtime/ImportPipeline.h
        include/${CLASS_NAME}/Runtime/ScenePreloader.h

        include/${CLASS_NAME}/Types/ResourceType.h
        include/${CLASS_NAME}/Types/ResourceAnimation/ResourceAnimation.h
        include/${CLASS_NAME}/Types/ResourceAnimation/ImporterAnimation.h
        include/${CLASS_NAME}/Types/ResourceAudio/ResourceAudio.h
        include/${CLASS_NAME}/Types/ResourceAudioGraph/ResourceAudioGraph.h
        include/${CLASS_NAME}/Types/ResourceAudioGraph/ImporterAudioGraph.h
        include/${CLASS_NAME}/Types/ResourceMaterial/ResourceMaterial.h
        include/${CLASS_NAME}/Types/ResourceMaterial/ImporterMaterial.h
        include/${CLASS_NAME}/Types/ResourceMesh/ResourceMesh.h
        include/${CLASS_NAME}/Types/ResourceMesh/ImporterMesh.h
        include/${CLASS_NAME}/Types/ResourceMesh/SubMeshCache.h
        include/${CLASS_NAME}/Types/ResourceShader/ResourceShader.h
        include/${CLASS_NAME}/Types/ResourceSkeleton/ResourceSkeleton.h
        include/${CLASS_NAME}/Types/ResourceSkeleton/ImporterSkeleton.h
        include/${CLASS_NAME}/Types/ResourceTexture/ResourceTexture.h
        include/${CLASS_NAME}/Types/ResourceVideo/ResourceVideo.h
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
        ShaderSystem
        FileWatcher       # HotReloader holds one
        MemoryManager     # NOUS_Vector in public resource headers
        Renderer          # IGPUResourceFactory / RendererTypes in public headers
        AudioSystem_headers  # ResourceAudioGraph.h names AudioGraphDesc
        Utils_headers     # ResourceMesh.h / ImporterMesh.h include <Utils/Math/Vertex.inl>
        AnimationSystem   # SkeletonBuild.h / ClipBuild.h expose SkeletonData +
                          # AnimClipData BY VALUE, and SkeletonBuild.cpp calls
                          # RebuildLookup, so this is needed in both halves.
                          # AnimationSystem links only glm, so no cycle: the edge
                          # runs ResourceManager -> AnimationSystem and stops.
)

# AudioSystem + VideoSystem close cycles: the Resource* types live here, the
# codecs and probes live there, and the importers call into them.
target_link_libraries(${CLASS_NAME} PRIVATE
        AudioSystem
        FileSystem
        Logger
        NOUS_Multithreading
        Utils
        VideoSystem
        assimp::assimp
)

add_subdirectory(src/Core)
add_subdirectory(src/Import)
add_subdirectory(src/Runtime)
add_subdirectory(src/Types)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
