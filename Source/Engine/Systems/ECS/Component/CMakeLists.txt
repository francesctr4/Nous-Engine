set(CLASS_NAME "Component")

target_sources(ECS PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)

add_subdirectory(CMaterial)
add_subdirectory(CMesh)
add_subdirectory(CScript)
add_subdirectory(CTransform)