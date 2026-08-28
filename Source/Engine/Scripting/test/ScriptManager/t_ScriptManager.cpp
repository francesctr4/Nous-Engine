// Covers ScriptManager's own logic. Its sibling
// t_Scripting_ScriptHotReloadPlatform covers the OS mechanism underneath
// (compile -> LoadLibrary/dlopen -> symbol -> unload) and links no engine code;
// this file is the class that orchestrates it, which had no coverage at all
// until now -- the platform test used to be NAMED t_ScriptManager, which made
// the suite look covered.
//
// The scope here is the NO-LIBRARY state: what ScriptManager does before any
// Scripts.dll is loaded, and after a failed load. That is the state the editor
// is in at startup, on every failed recompile, and in any game shipped without
// scripts, and every method has to survive it. It is also the half that needs no
// compiler, no DLL and no filesystem, so it is deterministic in CI.
//
// NOT covered, and worth saying why:
//   * A successful load/reload cycle needs a real compiled Scripts.dll. The
//     platform test already proves that machinery works end to end; porting it
//     onto ScriptManager would mean invoking the host compiler from this test
//     too. Worth doing, but it is a fixture, not a test.
//   * GenerateScript reads its template from the INSTALLED SDK at runtime
//     (GetExeDir()/EngineSDK/Scripts/ScriptTemplate.inl), so its success path
//     depends on whether InstallEngine has run into this build dir. Only its
//     deterministic failure path is asserted below; making the success path
//     testable would mean taking the template path as a parameter.

#include <gtest/gtest.h>

#include <Scripting/ScriptManager.h>
#include <Scripting/iScriptRegistry.h>

#include <MemoryManager/MemoryManager.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class t_ScriptManager : public ::testing::Test
{
protected:
    static constexpr uint64_t kMemoryPoolSize = MiB(16);

    void SetUp() override
    {
        // m_scriptComponents is a NOUS_Vector on the SCRIPTING_SYSTEM tag, so the
        // pool must exist before the manager is constructed.
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // Null input/sceneHost is the honest headless case: both are only ever
        // forwarded to ScriptBindings, and no binding runs without a DLL.
        manager = new ScriptManager(nullptr, nullptr);
    }

    void TearDown() override
    {
        delete manager;
        nous::engine::memory::ShutdownMemory();
    }

    [[nodiscard]] static std::string ScratchPath(const std::string& name)
    {
        const fs::path dir = fs::temp_directory_path() / "nous_t_scriptmanager";
        fs::create_directories(dir);
        return (dir / name).string();
    }

    ScriptManager* manager = nullptr;
};

// ---------------------------------------------------------------------------
// Construction / teardown with no library
// ---------------------------------------------------------------------------

TEST_F(t_ScriptManager, ConstructsWithoutALibrary)
{
    EXPECT_NE(manager, nullptr);
}

TEST_F(t_ScriptManager, DestructorUnloadsCleanlyWithNothingLoaded)
{
    // The destructor calls UnloadScriptLibrary unconditionally; with a null
    // handle that must be a no-op rather than an unload of garbage.
    ScriptManager* local = new ScriptManager(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(delete local);
}

TEST_F(t_ScriptManager, UnloadWithNothingLoadedIsANoOp)
{
    EXPECT_NO_FATAL_FAILURE(manager->UnloadScriptLibrary());
    EXPECT_NO_FATAL_FAILURE(manager->UnloadScriptLibrary());
}

// ---------------------------------------------------------------------------
// Loading failures
// ---------------------------------------------------------------------------

TEST_F(t_ScriptManager, LoadingAMissingLibraryFails)
{
    EXPECT_FALSE(manager->LoadScriptLibrary(ScratchPath("no_such_library.dll")));
}

TEST_F(t_ScriptManager, LoadingAnEmptyPathFails)
{
    EXPECT_FALSE(manager->LoadScriptLibrary(""));
}

TEST_F(t_ScriptManager, LoadingANonLibraryFileFails)
{
    // A file that exists but is not a loadable image -- the realistic case of a
    // truncated or half-written Scripts.dll from an interrupted compile.
    const std::string path = ScratchPath("garbage.dll");
    {
        FILE* f = fopen(path.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        fputs("this is not a portable executable", f);
        fclose(f);
    }

    EXPECT_FALSE(manager->LoadScriptLibrary(path));
}

TEST_F(t_ScriptManager, AFailedLoadLeavesTheManagerUsable)
{
    // The editor keeps running after a failed recompile, so a failed load must
    // leave the no-library state intact rather than a half-loaded one.
    EXPECT_FALSE(manager->LoadScriptLibrary(ScratchPath("missing.dll")));

    EXPECT_TRUE(manager->GetAvailableScriptNames().empty());
    EXPECT_EQ(manager->CreateScriptInstance("Anything"), nullptr);
    EXPECT_NO_FATAL_FAILURE(manager->UnloadScriptLibrary());
}

TEST_F(t_ScriptManager, ReloadingAMissingLibraryFails)
{
    EXPECT_FALSE(manager->ReloadScriptLibrary(ScratchPath("no_such_library.dll")));
}

TEST_F(t_ScriptManager, RepeatedFailedLoadsDoNotAccumulateState)
{
    for (int i = 0; i < 3; ++i)
        EXPECT_FALSE(manager->LoadScriptLibrary(ScratchPath("missing.dll")));

    EXPECT_TRUE(manager->GetAvailableScriptNames().empty());
}

// ---------------------------------------------------------------------------
// Queries with no registry
// ---------------------------------------------------------------------------

TEST_F(t_ScriptManager, NoScriptsAreAvailableWithoutALibrary)
{
    EXPECT_TRUE(manager->GetAvailableScriptNames().empty());
}

TEST_F(t_ScriptManager, CreateScriptInstanceReturnsNullWithoutALibrary)
{
    // CScript::CreateInstances calls this for every serialized script name on
    // scene load. In a game shipped without scripts it must return null quietly
    // rather than dereference a null registry.
    EXPECT_EQ(manager->CreateScriptInstance("PlayerController"), nullptr);
}

TEST_F(t_ScriptManager, CreateScriptInstanceWithAnEmptyNameReturnsNull)
{
    EXPECT_EQ(manager->CreateScriptInstance(""), nullptr);
}

TEST_F(t_ScriptManager, IsUsableThroughIScriptRegistry)
{
    // CScript never holds a ScriptManager*; it sees an IScriptRegistry through
    // ComponentServices, so the no-library contract must hold through the
    // interface too.
    IScriptRegistry& registry = *manager;
    EXPECT_EQ(registry.CreateScriptInstance("PlayerController"), nullptr);
}

// ---------------------------------------------------------------------------
// Dispatch against an empty component registry
// ---------------------------------------------------------------------------

TEST_F(t_ScriptManager, DispatchesAreNoOpsWithNoRegisteredComponents)
{
    EXPECT_NO_FATAL_FAILURE(manager->DispatchLateUpdate(0.016f));
    EXPECT_NO_FATAL_FAILURE(manager->DispatchFixedUpdate(0.02f));
}

TEST_F(t_ScriptManager, LifecycleSweepsAreNoOpsWithNoRegisteredComponents)
{
    EXPECT_NO_FATAL_FAILURE(manager->RecreateAllInstances());
    EXPECT_NO_FATAL_FAILURE(manager->StartAllInstances());
}

TEST_F(t_ScriptManager, CleanupWithNothingRegisteredIsANoOp)
{
    EXPECT_NO_FATAL_FAILURE(manager->CleanupScripts());
}

TEST_F(t_ScriptManager, CleanupIsIdempotent)
{
    // ModuleScene::CleanUp calls this, and the destructor runs afterwards.
    manager->CleanupScripts();
    EXPECT_NO_FATAL_FAILURE(manager->CleanupScripts());
}

TEST_F(t_ScriptManager, DispatchAfterCleanupIsStillSafe)
{
    // Ordering during shutdown is not guaranteed to be tidy: a queued
    // PostUpdate can still land after CleanupScripts.
    manager->CleanupScripts();
    EXPECT_NO_FATAL_FAILURE(manager->DispatchLateUpdate(0.016f));
    EXPECT_NO_FATAL_FAILURE(manager->DispatchFixedUpdate(0.02f));
}

TEST_F(t_ScriptManager, ZeroAndNegativeDeltaTimesAreTolerated)
{
    // A paused frame reports dt == 0; a clock adjustment can briefly report a
    // negative one. Neither may be special-cased into a crash.
    EXPECT_NO_FATAL_FAILURE(manager->DispatchLateUpdate(0.f));
    EXPECT_NO_FATAL_FAILURE(manager->DispatchFixedUpdate(0.f));
    EXPECT_NO_FATAL_FAILURE(manager->DispatchLateUpdate(-1.f));
}

// ---------------------------------------------------------------------------
// Script generation -- deterministic failure path only (see the header note)
// ---------------------------------------------------------------------------

TEST_F(t_ScriptManager, GenerateScriptFailsWhenTheOutputDirectoryDoesNotExist)
{
    // std::ofstream does not create intermediate directories, so this fails at
    // the write regardless of whether the SDK template was found.
    EXPECT_FALSE(ScriptManager::GenerateScript(
        "Ghost", ScratchPath("no/such/nested/directory")));
}

TEST_F(t_ScriptManager, GenerateScriptDoesNotCreateAFileWhenItFails)
{
    const std::string dir = ScratchPath("gen_fail");
    fs::create_directories(dir);
    const fs::path expected = fs::path(dir) / "Ghost.cpp";
    fs::remove(expected);

    // Succeeds only if the installed SDK template is present next to the test
    // binary. Either way the postcondition is the same: a file exists if and
    // only if the call reported success.
    const bool ok = ScriptManager::GenerateScript("Ghost", dir);
    EXPECT_EQ(ok, fs::exists(expected));
}
