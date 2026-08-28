# ==============================
# Nous Engine - Scripting Tests
# ==============================

set(TEST_NAME "t_ScriptManager")

# -----------------------------------------------------------------------
# Bake the compiler path and MSVC lib dirs into the test binary so it can
# invoke the compiler without needing vcvars or a special PATH at run time.
# -----------------------------------------------------------------------
file(TO_CMAKE_PATH "${CMAKE_CXX_COMPILER}" NOUS_TEST_CXX_COMPILER)

if(MSVC)
    # CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES is often empty for MSVC because CMake
    # relies on vcvars to set LIB at build time and never stores it. Derive the
    # required lib directories from the compiler path instead — the MSVC layout
    # is always: <MSVC_ROOT>/bin/Host<X>/<arch>/cl.exe
    #             <MSVC_ROOT>/lib/<arch>/   ← what we need
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _cl_dir)    # .../bin/HostX64/x64
    cmake_path(GET _cl_dir            PARENT_PATH _host_dir)  # .../bin/HostX64
    cmake_path(GET _host_dir          PARENT_PATH _bin_dir)   # .../bin
    cmake_path(GET _bin_dir           PARENT_PATH _msvc_root) # .../VC/Tools/MSVC/<ver>

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch "x64")
    else()
        set(_arch "x86")
    endif()

    set(_lib_dirs "${_msvc_root}/lib/${_arch}")

    # Windows SDK lib dirs (ucrt + um) — available when CMake targets Windows 10+
    if(CMAKE_WINDOWS_KITS_10_DIR AND CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(_sdk_base "${CMAKE_WINDOWS_KITS_10_DIR}/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
        list(APPEND _lib_dirs
             "${_sdk_base}/ucrt/${_arch}"
             "${_sdk_base}/um/${_arch}"
        )
    endif()

    string(JOIN ";" NOUS_TEST_MSVC_LIB_DIRS ${_lib_dirs})
else()
    set(NOUS_TEST_MSVC_LIB_DIRS "")
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/TestCompilerInfo.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/TestCompilerInfo.h"
        @ONLY
)

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

target_include_directories(${TEST_NAME} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"   # for TestCompilerInfo.h
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
)

# dlopen / dlclose on Linux & macOS
if(UNIX)
    target_link_libraries(${TEST_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(GoogleTest)
add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
