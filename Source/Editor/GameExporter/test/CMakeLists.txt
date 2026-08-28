set(TEST_NAME "t_ModuleEditor_GameExporter")

add_executable(${TEST_NAME}
        t_GameExporter.cpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # Narrow link: the editor archive under test, plus the engine as a DLL.
        #
        # The engine CANNOT be linked as static archives here. EditorUI/ModuleEditor
        # objects are compiled without NOUS_ENGINE_EXPORTS, so their references to
        # engine symbols are __imp_-prefixed and only an import library satisfies
        # them. Linking engine archives alongside the DLL is worse than useless: it
        # gives the test two copies of every engine global -- two memory pools, two
        # logger states -- which is what the LNK4217 warnings were pointing at.
        #
        # So only NOUS_EDITOR_STATIC is defined below, not NOUS_ENGINE_STATIC:
        # editor symbols are linked in, engine symbols are still imported.
        ModuleEditor
        NousEngine::Engine
        GTest::gtest
        GTest::gtest_main
)

# Editor code is linked INTO this binary; engine code is not. See
# EditorCore/EditorExport.h.
target_compile_definitions(${TEST_NAME} PRIVATE NOUS_EDITOR_STATIC)

include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
