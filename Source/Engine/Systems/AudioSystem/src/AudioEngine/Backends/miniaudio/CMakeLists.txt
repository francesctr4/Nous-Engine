target_sources(AudioSystem PRIVATE
        MiniaudioBackend.cpp
        MiniaudioBackend.h
        MiniaudioBusGraph.cpp
        MiniaudioBusGraph.h
        MiniaudioEffectChain.cpp
        MiniaudioEffectChain.h
        MiniaudioGainNode.cpp
        MiniaudioGainNode.h
        MiniaudioReverbNode.cpp
)

# Include miniaudio
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(AudioSystem PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

# Include stb (for stb_vorbis.c — enables .ogg decoding in miniaudio)
find_path(STB_INCLUDE_DIRS "stb_vorbis.c")
target_include_directories(AudioSystem PRIVATE ${STB_INCLUDE_DIRS})

# ----------------------------------------
# ma_reverb_node + verblib (three-file download → ThirdParty/)
#
# ma_reverb_node is NOT part of miniaudio.h — it lives in the upstream repo's
# extras/nodes/ and wraps verblib, so the vcpkg package does not install it.
# Same configure-time download pattern as ThirdParty/ImGuiColorTextEdit.
# ----------------------------------------
set(MA_NODES_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/MiniaudioNodes")
set(MA_NODES_BASE_URL "https://raw.githubusercontent.com/mackron/miniaudio/master/extras/nodes/ma_reverb_node")

file(MAKE_DIRECTORY ${MA_NODES_DIR})

foreach(FILE ma_reverb_node.h ma_reverb_node.c verblib.h)
    if(NOT EXISTS "${MA_NODES_DIR}/${FILE}")
        message(STATUS "Downloading MiniaudioNodes/${FILE}...")
        file(DOWNLOAD
            "${MA_NODES_BASE_URL}/${FILE}"
            "${MA_NODES_DIR}/${FILE}"
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
        if(NOT DOWNLOAD_CODE EQUAL 0)
            # Remove the stub a failed download leaves behind, or the next configure
            # sees the file as present and compiles an empty translation unit.
            file(REMOVE "${MA_NODES_DIR}/${FILE}")
            message(FATAL_ERROR "Failed to download MiniaudioNodes/${FILE}")
        endif()
    endif()
endforeach()

# ma_reverb_node.h reaches for miniaudio.h by a path relative to the upstream repo
# ("../../../miniaudio.h"), which does not exist outside that tree. Rewrite it to the
# include-dir spelling vcpkg provides. Guarded so an already-patched file is left
# untouched — an unconditional write would bump the mtime and rebuild every configure.
file(READ "${MA_NODES_DIR}/ma_reverb_node.h" MA_REVERB_HEADER_IN)
string(REPLACE
    "#include \"../../../miniaudio.h\""
    "#include <miniaudio.h>"
    MA_REVERB_HEADER_OUT "${MA_REVERB_HEADER_IN}"
)
if(NOT MA_REVERB_HEADER_IN STREQUAL MA_REVERB_HEADER_OUT)
    file(WRITE "${MA_NODES_DIR}/ma_reverb_node.h" "${MA_REVERB_HEADER_OUT}")
endif()

# SYSTEM so verblib's warnings are not ours.
target_include_directories(AudioSystem SYSTEM PRIVATE ${MA_NODES_DIR})
