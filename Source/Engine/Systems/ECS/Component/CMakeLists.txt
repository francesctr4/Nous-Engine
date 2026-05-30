set(CLASS_NAME "Component")

target_sources(ECS PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
        ComponentList.h
        Types/ComponentTypes.h
)

add_subdirectory(Types)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()