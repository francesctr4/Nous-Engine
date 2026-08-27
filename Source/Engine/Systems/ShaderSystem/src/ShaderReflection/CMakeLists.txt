set(CLASS_NAME "ShaderReflection")

# Private half only: this unit's three headers are API and live under
# include/ShaderSystem/ShaderReflection/, listed in the target root's CMakeLists.
target_sources(ShaderSystem PRIVATE
        ${CLASS_NAME}.cpp
        ${CLASS_NAME}Serializer.cpp
)
