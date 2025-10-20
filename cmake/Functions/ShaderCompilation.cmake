# ============================================
# ShaderCompilation.cmake — Vulkan GLSL → SPIR-V
# ============================================

function(setup_shader_compilation TARGET_NAME)
    find_program(GLSLC_EXECUTABLE
            NAMES glslc
            HINTS ${Vulkan_GLSLC_EXECUTABLE}
            DOC "Path to the glslc shader compiler"
    )

    if(NOT GLSLC_EXECUTABLE)
        message(WARNING "glslc not found. Shaders will not be compiled automatically.")
        return()
    endif()

    message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Assets/Shaders")
    set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/bin/Library/Shaders")

    file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
            "${SHADER_SOURCE_DIR}/*.glsl"
    )

    set(COMPILED_SHADERS)
    foreach(SHADER_FILE ${SHADER_FILES})
        file(RELATIVE_PATH SHADER_REL_PATH ${SHADER_SOURCE_DIR} ${SHADER_FILE})
        string(REGEX REPLACE "\\.glsl$" ".spv" SHADER_REL_PATH_SPV ${SHADER_REL_PATH})
        set(SHADER_OUTPUT_FILE "${SHADER_BUILD_DIR}/${SHADER_REL_PATH_SPV}")
        get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT_FILE} DIRECTORY)

        # Detect shader stage
        set(SHADER_STAGE "")
        if(SHADER_REL_PATH MATCHES "\\.vert\\.")
            set(SHADER_STAGE "vertex")
        elseif(SHADER_REL_PATH MATCHES "\\.frag\\.")
            set(SHADER_STAGE "fragment")
        elseif(SHADER_REL_PATH MATCHES "\\.comp\\.")
            set(SHADER_STAGE "compute")
        elseif(SHADER_REL_PATH MATCHES "\\.geom\\.")
            set(SHADER_STAGE "geometry")
        elseif(SHADER_REL_PATH MATCHES "\\.tesc\\.")
            set(SHADER_STAGE "tesscontrol")
        elseif(SHADER_REL_PATH MATCHES "\\.tese\\.")
            set(SHADER_STAGE "tesseval")
        else()
            message(WARNING "Unknown shader stage for ${SHADER_REL_PATH}. Skipping...")
            continue()
        endif()

        add_custom_command(
                OUTPUT ${SHADER_OUTPUT_FILE}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
                COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=${SHADER_STAGE} "${SHADER_FILE}" -o "${SHADER_OUTPUT_FILE}"
                DEPENDS ${SHADER_FILE}
                COMMENT "Compiling ${SHADER_STAGE} shader: ${SHADER_REL_PATH}"
                VERBATIM
        )

        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT_FILE})
    endforeach()

    if(COMPILED_SHADERS)
        add_custom_target(CompileShaders ALL
                DEPENDS ${COMPILED_SHADERS}
                COMMENT "Compiling all shaders"
        )
        add_dependencies(${TARGET_NAME} CompileShaders)
    endif()
endfunction()