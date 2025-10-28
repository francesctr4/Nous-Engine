set(CLASS_NAME "ImGuiCustom")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(${CLASS_NAME} PUBLIC
        imgui::imgui
)
