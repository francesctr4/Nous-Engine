target_sources(ModuleEditor PRIVATE
        include/GameExporter.h
        src/GameExporter.cpp
)

if(NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
