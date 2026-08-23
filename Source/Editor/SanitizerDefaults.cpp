// ============================================================================
// SanitizerDefaults.cpp
//
// Built into EditorApp only when a sanitizer build is configured
// (NOUS_SANITIZE, see cmake/Config/Sanitizers.cmake).
//
// The sanitizer runtimes call these weak hooks during their own initialisation,
// before main(), and merge the returned string with anything in TSAN_OPTIONS /
// UBSAN_OPTIONS. That makes the suppression files apply no matter how the binary
// is launched — CLion, Konsole, or CI — with no environment set up by hand.
//
// This exists specifically so the run configuration does NOT have to define
// TSAN_OPTIONS: CLion injects its own log_path into that variable to populate
// the clickable "Sanitizers" tab, and overwriting it loses the parsed view.
// Environment settings still win over these defaults, so nothing here blocks a
// one-off override from a terminal.
// ============================================================================

#if defined(NOUS_TSAN_SUPPRESSIONS)

extern "C" const char* __tsan_default_options()
{
    // second_deadlock_stack=1 makes lock-order-inversion reports show BOTH
    // acquisition stacks; without it you only get one half and the report is
    // usually not actionable.
    // history_size is the shadow-memory depth used to recover the "previous
    // access" stack. 7 is the maximum and costs memory, which is affordable
    // here and avoids "failed to restore the stack" on the older access.
    return "suppressions=" NOUS_TSAN_SUPPRESSIONS ":second_deadlock_stack=1:history_size=7";
}

#endif // NOUS_TSAN_SUPPRESSIONS

#if defined(NOUS_UBSAN_ENABLED)

extern "C" const char* __ubsan_default_options()
{
    return "print_stacktrace=1";
}

#endif // NOUS_UBSAN_ENABLED
