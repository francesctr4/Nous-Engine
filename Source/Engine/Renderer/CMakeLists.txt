# ==============================
# Nous Engine - Renderer (shared types)
#
# Public/private header layout (see CONVENTIONS.md).
#
#   include/Renderer/**  is the API, reached as <Renderer/RendererTypes.h>.
#   src/**               is implementation.
#
# Four targets live under Engine/Renderer/, and each carries its own prefix:
# <Renderer/...>, <RendererFrontend/...>, <RendererBackend/...>,
# <VulkanBackend/...>. The prefix names the target that owns the header, which is
# what makes "who may include this" answerable from the include line alone. A
# single shared Renderer/ prefix across all four would need two include dirs
# claiming one prefix and would hide that ownership.
#
# All five headers here are API and there is no private one -- this target exists
# precisely to be depended upon.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================

set(CLASS_NAME "Renderer")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(Backend)
add_subdirectory(Frontend)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}Types.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/RendererTypes.h
        include/${CLASS_NAME}/IGPUResourceFactory.h
        include/${CLASS_NAME}/iEditorRenderBridge.h
        include/${CLASS_NAME}/iRenderWindow.h
        include/${CLASS_NAME}/iRenderResourceProvider.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Renderer is the SHARED-TYPES target, not an aggregate: it compiles RendererTypes
# and owns the five interface headers (iRenderWindow, iRenderResourceProvider,
# iEditorRenderBridge, IGPUResourceFactory) that the frontend, the backend and
# their consumers all depend on. It therefore sits at the BOTTOM of the renderer
# graph and must NOT link RendererFrontend/RendererBackend -- doing so made
# Renderer <-> RendererFrontend <-> RendererBackend a cycle, for the same reason
# Modules <-> ModuleX was one. Consumers link the half they use.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm          # RendererTypes.h is glm-typed throughout
        # iEditorRenderBridge.h is deliberately Vulkan-typed -- ~9 raw handles are
        # irreducibly required by ImGui_ImplVulkan_Init -- so it includes
        # <vulkan/vulkan.h> from a PUBLIC header and the dependency is real. It was
        # never declared: every vcpkg package installs into one include dir, so the
        # vulkan headers happened to be on the path already.
        Vulkan::Vulkan
)
