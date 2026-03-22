#include <gtest/gtest.h>

#include "TestCompilerInfo.h"   // NOUS_TEST_CXX_COMPILER, NOUS_TEST_MSVC_LIB_DIRS

#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#  include <Windows.h>
#else
#  include <dlfcn.h>
#  include <cstdio>   // popen / pclose
#endif

// ============================================================
// Platform helpers
// ============================================================

namespace
{

void* LoadLib(const std::string& path)
{
#ifdef _WIN32
    return LoadLibraryA(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

void* GetSym(void* handle, const char* name)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void UnloadLib(void* handle)
{
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

const char* SharedLibExt()
{
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

// Returns true if the compiler baked in by CMake is present on disk.
// This check requires no process execution and never gives false negatives.
bool IsCompilerAvailable()
{
    return std::filesystem::exists(NOUS_TEST_CXX_COMPILER);
}

#ifdef _WIN32
// Add the compiler's directory to PATH so cl.exe can locate link.exe.
// We do NOT need to set LIB because the test DLL is compiled with /NODEFAULTLIB,
// which means the linker requires no runtime libraries at all.
void SetupMSVCEnvironment()
{
    static bool done = false;
    if (done) return;
    done = true;

    const std::string compilerDir =
        std::filesystem::path(NOUS_TEST_CXX_COMPILER).parent_path().string();

    char pathBuf[32767] = {};
    GetEnvironmentVariableA("PATH", pathBuf, sizeof(pathBuf));

    if (std::string(pathBuf).find(compilerDir) == std::string::npos)
    {
        const std::string newPath = compilerDir + ";" + pathBuf;
        SetEnvironmentVariableA("PATH", newPath.c_str());
    }
}
#endif

// Compile srcPath into a shared library at outPath.
// Uses the full compiler path baked in by CMake — no vcvars or PATH required.
// Returns true on success, false if compilation fails for any reason.
// Callers must check IsCompilerAvailable() first and skip if false.
//
// On Windows: srcPath must be a .c file. We compile with /NODEFAULTLIB /ENTRY:DllMain
// so the linker requires NO runtime libraries — no LIB environment variable needed.
bool CompileSharedLib(const std::string& srcPath, const std::string& outPath)
{
#ifdef _WIN32
    SetupMSVCEnvironment();

    // /GS-: disable buffer security check (__security_check_cookie) — the only CRT
    //       symbol MSVC injects into trivial functions when /GS is on (default).
    // /NODEFAULTLIB: no CRT — eliminates the need for the LIB environment variable.
    // /ENTRY:DllMain: linker flag — use our hand-rolled DllMain as the PE entry point
    //   instead of _DllMainCRTStartup. MUST go after /link, not before /Fe.
    // cmd.exe outer-quote syntax: ""{exe}" args" — outer quotes stripped by cmd.exe,
    // inner quotes protect the executable path from spaces.
    const std::string logPath = outPath + ".build.log";
    const std::string cmd =
        "\"\"" NOUS_TEST_CXX_COMPILER "\""
        " /nologo /LD /GS-"
        " /Fe:\"" + outPath + "\""
        " \"" + srcPath + "\""
        " /link /NODEFAULTLIB /ENTRY:DllMain"
        " >\"" + logPath + "\" 2>&1\"";

    const int rc = std::system(cmd.c_str());
    if (rc != 0)
    {
        // Print compiler output so the failure message contains actionable info.
        std::ifstream log(logPath);
        if (log.is_open())
        {
            std::string line;
            while (std::getline(log, line))
                std::cerr << "[cl.exe] " << line << "\n";
        }
    }
    std::filesystem::remove(logPath);
    return rc == 0;

#else
    // Full path to g++/clang++ (forward slashes, quotes handle spaces).
    const std::string cmd =
        "\"" NOUS_TEST_CXX_COMPILER "\""
        " -std=c++17 -shared -fPIC"
        " -o \"" + outPath + "\""
        " \"" + srcPath + "\""
        " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) { /* drain */ }
    return pclose(pipe) == 0;
#endif
}

} // namespace

// ============================================================
// DLL loading / symbol resolution
// ============================================================

TEST(t_ScriptManager, LoadNonExistentLibrary)
{
    void* handle = LoadLib("/nonexistent/path/Scripts.so");
    EXPECT_EQ(handle, nullptr);
}

// ============================================================
// Compilation integration tests
// ============================================================

TEST(t_ScriptManager, CompileLoadCallUnload)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_test";
    fs::create_directories(tmpDir);

    // On Windows we write a .c file so we can use /NODEFAULTLIB (no C++ name mangling needed).
    // On other platforms a .cpp file is fine.
#ifdef _WIN32
    const std::string srcPath = (tmpDir / "test_script.c").string();
#else
    const std::string srcPath = (tmpDir / "test_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("test_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        // Pure-C, no CRT: define BOOL/DWORD/etc. manually so /NODEFAULTLIB works.
        f << "typedef int BOOL;\n"
          << "typedef unsigned long DWORD;\n"
          << "typedef void* HINSTANCE;\n"
          << "typedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousTestGetMagic(void) { return 1337; }\n";
#else
        f << "#ifdef _WIN32\n"
          << "#  define EXPORT __declspec(dllexport)\n"
          << "#else\n"
          << "#  define EXPORT __attribute__((visibility(\"default\")))\n"
          << "#endif\n"
          << "extern \"C\" EXPORT int NousTestGetMagic() { return 1337; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath)) << "Compilation failed unexpectedly";
    ASSERT_TRUE(fs::exists(libPath)) << "Output library not found after successful compile";

    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr) << "Failed to load freshly compiled library";

    using MagicFn = int(*)();
    auto fn = reinterpret_cast<MagicFn>(GetSym(handle, "NousTestGetMagic"));
    ASSERT_NE(fn, nullptr) << "Symbol NousTestGetMagic not found";
    EXPECT_EQ(fn(), 1337);

    UnloadLib(handle);
    fs::remove(srcPath);
    fs::remove(libPath);
}

TEST(t_ScriptManager, ReloadCycle)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_reload_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "reload_script.c").string();
#else
    const std::string srcPath = (tmpDir / "reload_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("reload_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\n"
          << "typedef unsigned long DWORD;\n"
          << "typedef void* HINSTANCE;\n"
          << "typedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousTestVersion(void) { return 1; }\n";
#else
        f << "#ifdef _WIN32\n"
          << "#  define EXPORT __declspec(dllexport)\n"
          << "#else\n"
          << "#  define EXPORT __attribute__((visibility(\"default\")))\n"
          << "#endif\n"
          << "extern \"C\" EXPORT int NousTestVersion() { return 1; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));

    void* h1 = LoadLib(libPath);
    ASSERT_NE(h1, nullptr);
    auto fn1 = reinterpret_cast<int(*)()>(GetSym(h1, "NousTestVersion"));
    ASSERT_NE(fn1, nullptr);
    EXPECT_EQ(fn1(), 1);
    UnloadLib(h1);

    // Second load — the hot-reload invariant
    void* h2 = LoadLib(libPath);
    ASSERT_NE(h2, nullptr) << "Failed to reload library after unload";
    auto fn2 = reinterpret_cast<int(*)()>(GetSym(h2, "NousTestVersion"));
    ASSERT_NE(fn2, nullptr);
    EXPECT_EQ(fn2(), 1);
    UnloadLib(h2);

    fs::remove(srcPath);
    fs::remove(libPath);
}

TEST(t_ScriptManager, InvalidSourceFails)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_error_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "broken_script.c").string();
#else
    const std::string srcPath = (tmpDir / "broken_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("broken_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
        f << "this is not valid C++ code {{{;\n";
    }

    EXPECT_FALSE(CompileSharedLib(srcPath, libPath))
        << "Broken source should fail to compile";

    fs::remove(srcPath);
}

TEST(t_ScriptManager, HotReloadWithCodeChange)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_hotreload_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "hotreload_script.c").string();
#else
    const std::string srcPath = (tmpDir / "hotreload_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("hotreload_script") + SharedLibExt())).string();

    // --- Version 1 ---
    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousGetVersion(void) { return 1; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousGetVersion() { return 1; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* h1 = LoadLib(libPath);
    ASSERT_NE(h1, nullptr);
    auto fn1 = reinterpret_cast<int(*)()>(GetSym(h1, "NousGetVersion"));
    ASSERT_NE(fn1, nullptr);
    EXPECT_EQ(fn1(), 1);
    UnloadLib(h1);

    // --- Version 2 — overwrite source and recompile ---
    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousGetVersion(void) { return 2; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousGetVersion() { return 2; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath)) << "Recompile after code change failed";
    void* h2 = LoadLib(libPath);
    ASSERT_NE(h2, nullptr) << "Failed to load updated library";
    auto fn2 = reinterpret_cast<int(*)()>(GetSym(h2, "NousGetVersion"));
    ASSERT_NE(fn2, nullptr);
    EXPECT_EQ(fn2(), 2) << "Updated library should return new value after hot-reload";
    UnloadLib(h2);

    fs::remove(srcPath);
    fs::remove(libPath);
}

TEST(t_ScriptManager, FileUnlockedAfterUnload)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_unlock_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "unlock_script.c").string();
#else
    const std::string srcPath = (tmpDir / "unlock_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("unlock_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousTestGetMagic(void) { return 42; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousTestGetMagic() { return 42; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr);
    UnloadLib(handle);

    // After unloading, the OS must have released the file lock so we can overwrite it.
    // On Windows a loaded DLL is locked; this verifies FreeLibrary fully releases it.
    std::error_code ec;
    EXPECT_TRUE(fs::remove(libPath, ec))
        << "DLL file still locked after UnloadLib — hot-reload would fail: " << ec.message();

    fs::remove(srcPath);
}

TEST(t_ScriptManager, SymbolNotFound)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_sym_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "sym_script.c").string();
#else
    const std::string srcPath = (tmpDir / "sym_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("sym_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousRealFunction(void) { return 0; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousRealFunction() { return 0; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr);

    EXPECT_EQ(GetSym(handle, "NonExistentFunction"), nullptr)
        << "GetSym must return nullptr for an unknown symbol — not crash";
    EXPECT_NE(GetSym(handle, "NousRealFunction"), nullptr)
        << "GetSym must still find real exports after a failed lookup";

    UnloadLib(handle);
    fs::remove(srcPath);
    fs::remove(libPath);
}

TEST(t_ScriptManager, CompileToNonexistentDirectory)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_nodir_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "nodir_script.c").string();
#else
    const std::string srcPath = (tmpDir / "nodir_script.cpp").string();
#endif
    // Output directory does not exist — compiler/linker must fail, not silently succeed.
    const std::string badLibPath =
        (tmpDir / "no_such_subdir" / (std::string("out") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousTestGetMagic(void) { return 0; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousTestGetMagic() { return 0; }\n";
#endif
    }

    EXPECT_FALSE(CompileSharedLib(srcPath, badLibPath))
        << "Compiling to a nonexistent output directory should fail";

    fs::remove(srcPath);
}

TEST(t_ScriptManager, MultipleExports)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_multi_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "multi_script.c").string();
#else
    const std::string srcPath = (tmpDir / "multi_script.cpp").string();
#endif
    const std::string libPath = (tmpDir / (std::string("multi_script") + SharedLibExt())).string();

    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousOnStart(void)   { return 10; }\n"
          << "__declspec(dllexport) int NousOnUpdate(void)  { return 20; }\n"
          << "__declspec(dllexport) int NousOnDestroy(void) { return 30; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousOnStart()   { return 10; }\n"
          << "extern \"C\" EXPORT int NousOnUpdate()  { return 20; }\n"
          << "extern \"C\" EXPORT int NousOnDestroy() { return 30; }\n";
#endif
    }

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr);

    using Fn = int(*)();
    const auto fnStart   = reinterpret_cast<Fn>(GetSym(handle, "NousOnStart"));
    const auto fnUpdate  = reinterpret_cast<Fn>(GetSym(handle, "NousOnUpdate"));
    const auto fnDestroy = reinterpret_cast<Fn>(GetSym(handle, "NousOnDestroy"));

    ASSERT_NE(fnStart,   nullptr) << "NousOnStart not found";
    ASSERT_NE(fnUpdate,  nullptr) << "NousOnUpdate not found";
    ASSERT_NE(fnDestroy, nullptr) << "NousOnDestroy not found";

    EXPECT_EQ(fnStart(),   10);
    EXPECT_EQ(fnUpdate(),  20);
    EXPECT_EQ(fnDestroy(), 30);

    UnloadLib(handle);
    fs::remove(srcPath);
    fs::remove(libPath);
}

TEST(t_ScriptManager, ConcurrentLoads)
{
    if (!IsCompilerAvailable())
        GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;

    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "nous_script_concurrent_test";
    fs::create_directories(tmpDir);

#ifdef _WIN32
    const std::string srcA = (tmpDir / "concurrent_a.c").string();
    const std::string srcB = (tmpDir / "concurrent_b.c").string();
#else
    const std::string srcA = (tmpDir / "concurrent_a.cpp").string();
    const std::string srcB = (tmpDir / "concurrent_b.cpp").string();
#endif
    const std::string libA = (tmpDir / (std::string("concurrent_a") + SharedLibExt())).string();
    const std::string libB = (tmpDir / (std::string("concurrent_b") + SharedLibExt())).string();

    auto writeSource = [](const std::string& path, int returnVal) -> bool
    {
        std::ofstream f(path);
        if (!f.is_open()) return false;
#ifdef _WIN32
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\ntypedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n"
          << "__declspec(dllexport) int NousGetId(void) { return " << returnVal << "; }\n";
#else
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n"
          << "extern \"C\" EXPORT int NousGetId() { return " << returnVal << "; }\n";
#endif
        return true;
    };

    ASSERT_TRUE(writeSource(srcA, 100));
    ASSERT_TRUE(writeSource(srcB, 200));
    ASSERT_TRUE(CompileSharedLib(srcA, libA));
    ASSERT_TRUE(CompileSharedLib(srcB, libB));

    // Load both simultaneously — no global state should leak between modules.
    void* hA = LoadLib(libA);
    void* hB = LoadLib(libB);
    ASSERT_NE(hA, nullptr) << "Failed to load library A";
    ASSERT_NE(hB, nullptr) << "Failed to load library B";

    const auto fnA = reinterpret_cast<int(*)()>(GetSym(hA, "NousGetId"));
    const auto fnB = reinterpret_cast<int(*)()>(GetSym(hB, "NousGetId"));
    ASSERT_NE(fnA, nullptr);
    ASSERT_NE(fnB, nullptr);

    EXPECT_EQ(fnA(), 100) << "Library A returned wrong value";
    EXPECT_EQ(fnB(), 200) << "Library B returned wrong value";

    UnloadLib(hA);
    UnloadLib(hB);
    fs::remove(srcA); fs::remove(libA);
    fs::remove(srcB); fs::remove(libB);
}
