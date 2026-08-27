set(CLASS_NAME "ShaderLoader")

# Private half only: this unit's headers are API and live under
# include/ShaderSystem/ShaderLoader/, listed in the target root's CMakeLists.
target_sources(ShaderSystem PRIVATE
        ${CLASS_NAME}.cpp
)
