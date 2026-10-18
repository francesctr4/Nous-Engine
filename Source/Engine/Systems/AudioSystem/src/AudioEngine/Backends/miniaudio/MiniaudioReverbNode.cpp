// Compiles the vendored ma_reverb_node + verblib implementations.
//
// ma_reverb_node ships as a .c in miniaudio's extras/, not inside miniaudio.h.
// It is pulled in from a .cpp rather than added to the target as a C source so it
// compiles in the SAME language as MiniaudioBackend.cpp, which is where
// MA_IMPLEMENTATION lives. (Both resolve to C linkage either way — miniaudio.h and
// ma_reverb_node.h both wrap their declarations in extern "C" — but keeping one
// language means no C-language entry is needed in the project() call.)
//
// ma_reverb_node.c defines VERBLIB_IMPLEMENTATION itself, so verblib's definitions
// land here too and nowhere else. Do not include ma_reverb_node.c anywhere but here.

#include <ma_reverb_node.c>
