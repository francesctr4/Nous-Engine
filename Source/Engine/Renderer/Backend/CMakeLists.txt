# ==============================
# Nous Engine - Renderer Backend (the factory seam)
#
# include/RendererBackend/**  is the API: iRendererBackend.h (the pure-virtual
#                             backend interface) + RendererBackendFactory.h.
# src/RendererBackendFactory.cpp is the compile firewall -- see below.
#
# Both headers are API and there is no private one. VulkanBackend lives one level
# down as its own target and is NOT part of this one.
# ==============================

set(CLASS_NAME "RendererBackend")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(Vulkan)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}Factory.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/iRendererBackend.h
        include/${CLASS_NAME}/RendererBackendFactory.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# VulkanBackend is PUBLIC for linkage (the factory constructs one), but note that
# only ONE translation unit -- src/RendererBackendFactory.cpp -- may name
# VulkanBackend.h, and after the include/src split that is enforced rather than
# merely documented: 16 of VulkanBackend's 17 headers now live under its src/.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
        VulkanBackend
        Renderer
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
)
