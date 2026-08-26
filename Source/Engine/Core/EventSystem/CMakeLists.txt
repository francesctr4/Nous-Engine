set(CLASS_NAME "EventSystem")

add_library(${CLASS_NAME} STATIC
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
        IEventListener.h
)

add_subdirectory(Event)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC glm::glm)
target_link_libraries(${CLASS_NAME} PRIVATE Logger)