# ==============================
# Nous Engine - Utils
# ==============================

set(UTILS_SOURCES
        JsonFile.cpp
        JsonFile.h
        MathUtils.h
        Random.cpp
        Random.h
        Vertex.inl
)

add_library(Utils STATIC ${UTILS_SOURCES})

set_target_properties(Utils PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Utils PUBLIC ../..)

target_compile_definitions(Utils PRIVATE NOUS_ENGINE_EXPORTS)

find_package(glm REQUIRED)
target_link_libraries(Utils PUBLIC glm::glm)