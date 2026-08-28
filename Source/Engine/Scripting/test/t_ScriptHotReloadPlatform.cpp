#include <gtest/gtest.h>

#include "TestCompilerInfo.h"   // NOUS_TEST_CXX_COMPILER, NOUS_TEST_MSVC_LIB_DIRS

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <Windows.h>
#else
#  include <dlfcn.h>
#  include <cstdio>   // popen / pclose
#endif

// ============================================================
// What this covers -- and what it does NOT.
//
// This file tests the OPERATING SYSTEM MECHANISM that script hot-reload rests
// on, not the ScriptManager class. It deliberately links gtest and nothing else:
// compile a .cpp with the host compiler, load it (LoadLibrary / dlopen), resolve
// a symbol, call it, unload, edit, recompile, reload. Those are the assumptions
// the whole feature depends on and they are worth pinning per-platform.
//
// It was previously named t_ScriptManager, which made the suite look as though
// ScriptManager (652 lines, 16 public methods including ReloadScriptLibrary and
// the Windows shadow-copy) was covered. It is not -- see t_Scripting_ScriptManager
// for that. Renamed 2026-08-29 so a coverage audit reads the truth.
// ============================================================

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
//
// On Windows: srcPath must be a .c file. We compile with /NODEFAULTLIB /ENTRY:DllMain
// so the linker requires NO runtime libraries — no LIB environment variable needed.
// /GS-: disables the buffer security check (__security_check_cookie) that MSVC injects
//       by default, which would otherwise be an unresolved CRT symbol.
// /ENTRY:DllMain: linker flag (must follow /link) — bypasses _DllMainCRTStartup.
bool CompileSharedLib(const std::string& srcPath, const std::string& outPath)
{
#ifdef _WIN32
    SetupMSVCEnvironment();

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
// Test fixture
// ============================================================

class t_ScriptHotReloadPlatform : public ::testing::Test
{
protected:
    std::filesystem::path tmpDir;

    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tmpDir = std::filesystem::temp_directory_path() / "nous_script_tests" / info->name();
        std::filesystem::create_directories(tmpDir);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir);
    }

    // Skips the current test if the compiler is not available on disk.
    // Call at the top of every test that invokes CompileSharedLib.
    void RequireCompiler()
    {
        if (!IsCompilerAvailable())
            GTEST_SKIP() << "Compiler not found at: " NOUS_TEST_CXX_COMPILER;
    }

    // Returns the full path for a shared library with the given stem inside tmpDir.
    std::string LibPath(const std::string& stem) const
    {
        return (tmpDir / (stem + SharedLibExt())).string();
    }

    // Writes a minimal valid DLL source that exports the given {name, returnValue} pairs.
    // On Windows writes a .c file (for /NODEFAULTLIB compatibility).
    // On other platforms writes a .cpp file.
    // Returns the source file path, or empty string if writing failed.
    struct Export { std::string name; int value; };
    std::string WriteSource(const std::string& stem, const std::vector<Export>& exports) const
    {
#ifdef _WIN32
        std::string path = (tmpDir / (stem + ".c")).string();
        std::ofstream f(path);
        if (!f.is_open()) return {};
        f << "typedef int BOOL;\ntypedef unsigned long DWORD;\n"
          << "typedef void* HINSTANCE;\ntypedef void* LPVOID;\n"
          << "BOOL __stdcall DllMain(HINSTANCE h, DWORD r, LPVOID lp) { return 1; }\n";
        for (const auto& e : exports)
            f << "__declspec(dllexport) int " << e.name << "(void) { return " << e.value << "; }\n";
#else
        const std::string path = (tmpDir / (stem + ".cpp")).string();
        std::ofstream f(path);
        if (!f.is_open()) return {};
        f << "#define EXPORT __attribute__((visibility(\"default\")))\n";
        for (const auto& e : exports)
            f << "extern \"C\" EXPORT int " << e.name << "() { return " << e.value << "; }\n";
#endif
        return path;
    }
};

// ============================================================
// Tests
// ============================================================

TEST_F(t_ScriptHotReloadPlatform, LoadNonExistentLibrary)
{
    // No compiler needed — just verifies the loader returns null gracefully.
    void* handle = LoadLib("/nonexistent/path/Scripts.so");
    EXPECT_EQ(handle, nullptr);
}

TEST_F(t_ScriptHotReloadPlatform, CompileLoadCallUnload)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("test_script", {{"NousTestGetMagic", 1337}});
    ASSERT_FALSE(srcPath.empty());
    const std::string libPath = LibPath("test_script");

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath)) << "Compilation failed unexpectedly";
    ASSERT_TRUE(std::filesystem::exists(libPath)) << "Output library not found after successful compile";

    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr) << "Failed to load freshly compiled library";

    auto fn = reinterpret_cast<int(*)()>(GetSym(handle, "NousTestGetMagic"));
    ASSERT_NE(fn, nullptr) << "Symbol NousTestGetMagic not found";
    EXPECT_EQ(fn(), 1337);

    UnloadLib(handle);
}

TEST_F(t_ScriptHotReloadPlatform, ReloadCycle)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("reload_script", {{"NousTestVersion", 1}});
    ASSERT_FALSE(srcPath.empty());
    const std::string libPath = LibPath("reload_script");

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
}

TEST_F(t_ScriptHotReloadPlatform, InvalidSourceFails)
{
    RequireCompiler();

#ifdef _WIN32
    const std::string srcPath = (tmpDir / "broken_script.c").string();
#else
    const std::string srcPath = (tmpDir / "broken_script.cpp").string();
#endif
    {
        std::ofstream f(srcPath);
        ASSERT_TRUE(f.is_open());
        f << "this is not valid C++ code {{{;\n";
    }

    EXPECT_FALSE(CompileSharedLib(srcPath, LibPath("broken_script")))
        << "Broken source should fail to compile";
}

TEST_F(t_ScriptHotReloadPlatform, HotReloadWithCodeChange)
{
    RequireCompiler();

    const std::string libPath = LibPath("hotreload_script");

    // Version 1
    const std::string srcPath = WriteSource("hotreload_script", {{"NousGetVersion", 1}});
    ASSERT_FALSE(srcPath.empty());
    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));

    void* h1 = LoadLib(libPath);
    ASSERT_NE(h1, nullptr);
    auto fn1 = reinterpret_cast<int(*)()>(GetSym(h1, "NousGetVersion"));
    ASSERT_NE(fn1, nullptr);
    EXPECT_EQ(fn1(), 1);
    UnloadLib(h1);

    // Version 2 — overwrite source and recompile
    WriteSource("hotreload_script", {{"NousGetVersion", 2}});
    ASSERT_TRUE(CompileSharedLib(srcPath, libPath)) << "Recompile after code change failed";

    void* h2 = LoadLib(libPath);
    ASSERT_NE(h2, nullptr) << "Failed to load updated library";
    auto fn2 = reinterpret_cast<int(*)()>(GetSym(h2, "NousGetVersion"));
    ASSERT_NE(fn2, nullptr);
    EXPECT_EQ(fn2(), 2) << "Updated library should return new value after hot-reload";
    UnloadLib(h2);
}

TEST_F(t_ScriptHotReloadPlatform, FileUnlockedAfterUnload)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("unlock_script", {{"NousTestGetMagic", 42}});
    ASSERT_FALSE(srcPath.empty());
    const std::string libPath = LibPath("unlock_script");

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr);
    UnloadLib(handle);

    // After unloading, the OS must release the file lock so the DLL can be overwritten.
    // On Windows a loaded DLL is locked; this verifies FreeLibrary fully releases it.
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::remove(libPath, ec))
        << "DLL file still locked after UnloadLib — hot-reload would fail: " << ec.message();
}

TEST_F(t_ScriptHotReloadPlatform, SymbolNotFound)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("sym_script", {{"NousRealFunction", 0}});
    ASSERT_FALSE(srcPath.empty());
    const std::string libPath = LibPath("sym_script");

    ASSERT_TRUE(CompileSharedLib(srcPath, libPath));
    void* handle = LoadLib(libPath);
    ASSERT_NE(handle, nullptr);

    EXPECT_EQ(GetSym(handle, "NonExistentFunction"), nullptr)
        << "GetSym must return nullptr for an unknown symbol — not crash";
    EXPECT_NE(GetSym(handle, "NousRealFunction"), nullptr)
        << "GetSym must still find real exports after a failed lookup";

    UnloadLib(handle);
}

TEST_F(t_ScriptHotReloadPlatform, CompileToNonexistentDirectory)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("nodir_script", {{"NousTestGetMagic", 0}});
    ASSERT_FALSE(srcPath.empty());

    const std::string badLibPath =
        (tmpDir / "no_such_subdir" / (std::string("out") + SharedLibExt())).string();

    EXPECT_FALSE(CompileSharedLib(srcPath, badLibPath))
        << "Compiling to a nonexistent output directory should fail";
}

TEST_F(t_ScriptHotReloadPlatform, MultipleExports)
{
    RequireCompiler();

    const std::string srcPath = WriteSource("multi_script",
        {{"NousOnStart", 10}, {"NousOnUpdate", 20}, {"NousOnDestroy", 30}});
    ASSERT_FALSE(srcPath.empty());
    const std::string libPath = LibPath("multi_script");

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
}

TEST_F(t_ScriptHotReloadPlatform, ConcurrentLoads)
{
    RequireCompiler();

    const std::string srcA = WriteSource("concurrent_a", {{"NousGetId", 100}});
    const std::string srcB = WriteSource("concurrent_b", {{"NousGetId", 200}});
    ASSERT_FALSE(srcA.empty());
    ASSERT_FALSE(srcB.empty());
    const std::string libA = LibPath("concurrent_a");
    const std::string libB = LibPath("concurrent_b");

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
}
