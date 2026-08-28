# ==============================================================================
# Nous Engine - Utils
#
# Converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/Utils/**  is the API, reached as <Utils/Serialization/JsonFile.h>.
#   src/**            is implementation, reachable only from inside this target.
#
# Every header here is public -- Utils is a leaf of pure API, so the conversion
# buys no enforcement, exactly as with ShaderSystem and PrefabManager. What it
# does buy is that consumers stop resolving these headers through the global
# Source/ root, which is what has to be true before that root can be removed.
#
# Two shape notes:
#   * Serialization's JsonFile/ and Random/ sub-unit dirs collapse in include/
#     (four headers, one category) but are kept in src/ and test/, where the
#     per-leaf CMakeLists.txt marks the unit boundary.
#   * DataStructures/ and Math/ are header-only, so they have no src/ leaf at
#     all; their public headers are listed in the block below with the rest.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

add_library(Utils_headers INTERFACE)
target_include_directories(Utils_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(Utils_headers INTERFACE EngineCore_headers)

# NOUS_Vector.h is a public Utils header and routes STL containers through
# NOUS_STLAllocator, so anyone handed Utils' API dir needs MemoryManager's too.
# Headers only -- no objects, no transitive deps.
target_link_libraries(Utils_headers INTERFACE MemoryManager_headers)

add_library(Utils_private INTERFACE)
target_include_directories(Utils_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(Utils STATIC)

target_sources(Utils PRIVATE
        include/Utils/DataStructures/NOUS_Vector.h
        include/Utils/Math/FrustumCulling.h
        include/Utils/Math/MathUtils.h
        include/Utils/Math/Vertex.inl
        include/Utils/Serialization/JsonArray.h
        include/Utils/Serialization/JsonFile.h
        include/Utils/Serialization/JsonObject.h
        include/Utils/Serialization/Random.h
)

add_subdirectory(src/Serialization)

set_target_properties(Utils PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Utils
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Utils PRIVATE
        NOUS_ENGINE_EXPORTS
)

find_package(parson CONFIG REQUIRED)

target_link_libraries(Utils PUBLIC
        Utils_headers
        glm::glm
        parson::parson
)

target_link_libraries(Utils PRIVATE
        FileSystem
        Logger
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test/Math)
    add_subdirectory(test/Serialization/JsonFile)
    add_subdirectory(test/Serialization/Random)
endif()
