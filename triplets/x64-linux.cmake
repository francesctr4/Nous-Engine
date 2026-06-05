set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# ffmpeg ships hand-written assembly (nasm) objects that are NOT position-independent.
# Nous-Engine links its dependencies into a SHARED library (libNous-Engine.so), and a
# non-PIC static archive cannot be embedded into a .so:
#   relocation R_X86_64_PC32 ... can not be used when making a shared object; recompile with -fPIC
# Build ffmpeg as a shared library instead (its .so objects are PIC), leaving every other
# dependency static as before.
if(PORT STREQUAL "ffmpeg")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
