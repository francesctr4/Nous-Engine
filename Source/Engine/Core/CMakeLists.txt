# ==============================
# Nous Engine - Core
# ==============================

set(CORE_SOURCES
        Application.cpp
        Application.h
        Globals.h
        UpdateStatus.h
)

add_library(Core STATIC ${CORE_SOURCES})

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Include parent for "Core/..."
target_include_directories(Core PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Core PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Core
        PUBLIC
        glm::glm            # 👈 exported publicly
)
