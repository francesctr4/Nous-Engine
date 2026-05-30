set(CLASS_NAME "Component")

target_sources(ECS PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
        ComponentList.h
        ComponentTypes.h
)

add_subdirectory(CCamera)
add_subdirectory(CLight)
add_subdirectory(CMaterial)
add_subdirectory(CMesh)
add_subdirectory(CPrefab)
add_subdirectory(CScript)
add_subdirectory(CTransform)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()