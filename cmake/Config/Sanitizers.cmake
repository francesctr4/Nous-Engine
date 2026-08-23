# ============================================
# Sanitizers.cmake — GCC/Clang sanitizer wiring
# ============================================
#
#   cmake --preset ASan-Linux    -> NOUS_SANITIZE=address,undefined
#   cmake --preset TSan-Linux    -> NOUS_SANITIZE=thread
#
# ASan and TSan are mutually exclusive (they both claim the process shadow-memory
# mapping). UBSan combines with either. MSVC is not supported and is ignored.
#
# The flags are applied GLOBALLY on purpose. The engine builds as a shared library
# that dlopen()s Scripts.so; a partially instrumented process produces meaningless
# reports, so every translation unit in the project must agree.
#
# NOTE: vcpkg dependencies are NOT rebuilt with sanitizers. ASan copes with this
# via interposition, but TSan cannot see synchronization inside an uninstrumented
# library and will report false races through it — that is what
# Tools/Sanitizers/tsan.supp is for.

set(NOUS_SANITIZE "" CACHE STRING
        "Sanitizers to enable, comma separated (e.g. 'address,undefined' or 'thread'). Empty = off.")

set_property(CACHE NOUS_SANITIZE PROPERTY STRINGS
        "" "address" "address,undefined" "thread" "thread,undefined" "undefined")

if(NOUS_SANITIZE)
    if(MSVC)
        message(WARNING "NOUS_SANITIZE='${NOUS_SANITIZE}' ignored: sanitizers are not wired up for MSVC.")
        set(NOUS_SANITIZE "" CACHE STRING "" FORCE)
    else()
        if(NOUS_SANITIZE MATCHES "address" AND NOUS_SANITIZE MATCHES "thread")
            message(FATAL_ERROR
                    "NOUS_SANITIZE='${NOUS_SANITIZE}': 'address' and 'thread' are mutually exclusive. "
                    "Use one configure preset per sanitizer (ASan-Linux / TSan-Linux).")
        endif()

        # -O1 is deliberate and must come AFTER CMAKE_CXX_FLAGS_<CONFIG>. CMake emits
        # per-config flags first and COMPILE_OPTIONS after, so this overrides Debug's
        # -O0. Unoptimized sanitizer builds are slow enough to be unusable here, since
        # the editor already renders through Mesa llvmpipe on the VM.
        add_compile_options(-fsanitize=${NOUS_SANITIZE} -fno-omit-frame-pointer -g -O1)
        add_link_options(-fsanitize=${NOUS_SANITIZE})

        message(STATUS "Sanitizers ENABLED: -fsanitize=${NOUS_SANITIZE}")
    endif()
endif()
