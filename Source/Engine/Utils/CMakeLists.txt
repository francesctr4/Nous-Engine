# ==============================
# Nous Engine - Utils
# ==============================

set(UTILS_SOURCES
        Asserts.h
        JsonFile.cpp
        JsonFile.h
        Logger.cpp
        Logger.h
        MathUtils.h
        Random.cpp
        Random.h
)

add_library(Utils STATIC ${UTILS_SOURCES})
add_library(NousEngine::Utils ALIAS Utils)

set_target_properties(Utils PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Utils PUBLIC ../..)

find_package(glm REQUIRED)
target_link_libraries(Utils PUBLIC glm::glm)