# ============================================
# GlobalSettings.cmake — Core engine settings
# ============================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Build config macros
add_compile_definitions(
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:_RELEASE>
        $<$<CONFIG:Release>:NDEBUG>
)