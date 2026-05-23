target_sources(Utils PRIVATE
        JsonObject.h
        JsonObject.cpp
        JsonArray.h
        JsonArray.cpp
        JsonFile.h
        JsonFile.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()