#pragma once

#include <ECS/ComponentServices.h>
#include <ECS/Scene/iSceneHost.h>
#include <AudioSystem/iAudioBroker.h>
#include <VideoSystem/iVideoBroker.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <Scripting/iScriptRegistry.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Recording fakes for the ComponentServices seam.
// -----------------------------------------------------------------------------
// Each fake appends the name of every call it receives to `calls`, so a test can
// assert on the SEQUENCE of engine calls a component makes — which is the whole
// point of the interfaces. Before this seam existed, component tests could only
// assert "it doesn't crash".
//
// Header-only so any component test can include it without a link dependency.
//
// The const-qualified recorders use `mutable` state: the interface methods are
// const because the real modules forward to a backend without mutating module
// state. A fake, by definition, must record.

struct FakeSceneHost : ISceneHost
{
    bool  playing = false;
    bool  paused  = false;
    bool  loading = false;
    float aspect  = 16.0f / 9.0f;

    bool IsPlaying()      const override { return playing; }
    bool IsPaused()       const override { return paused; }
    bool IsStopped()      const override { return !playing && !paused; }
    bool IsLoadingScene() const override { return loading; }

    float   GetWindowAspect() const override { return aspect; }
    Camera* GetGameCamera()   const override { return nullptr; }

    // Convenience for tests that drive simulation edges.
    void SetPlaying() { playing = true;  paused = false; }
    void SetPaused()  { playing = false; paused = true;  }
    void SetStopped() { playing = false; paused = false; }
};

struct FakeAudioBroker : IAudioBroker
{
    mutable std::vector<std::string> calls;
    mutable std::uintptr_t           nextHandle = 1;

    mutable float                    lastVolume = -1.0f;
    mutable float                    lastPitch  = -1.0f;
    mutable std::array<float, 3>     lastPosition{ 0.0f, 0.0f, 0.0f };
    mutable std::array<float, 3>     lastListenerPosition{ 0.0f, 0.0f, 0.0f };
    mutable bool                     playing = false;
    mutable double                   cursor  = 0.0;

    bool Called(const char* name) const {
        return std::find(calls.begin(), calls.end(), name) != calls.end();
    }
    int CountOf(const char* name) const {
        return static_cast<int>(std::count(calls.begin(), calls.end(), name));
    }

    // ── Voice lifecycle ──
    SoundHandle CreateSound(ResourceAudio*, AudioBus) const override {
        calls.push_back("CreateSound");
        return reinterpret_cast<SoundHandle>(nextHandle++);
    }
    void DestroySound(SoundHandle) const noexcept override { calls.push_back("DestroySound"); }

    void StartSound(SoundHandle) const override { calls.push_back("StartSound"); playing = true; }
    void StopSound (SoundHandle) const override { calls.push_back("StopSound");  playing = false; }

    // ── Voice parameters ──
    void SetSoundVolume (SoundHandle, float v) const override { calls.push_back("SetSoundVolume");  lastVolume = v; }
    void SetSoundPitch  (SoundHandle, float p) const override { calls.push_back("SetSoundPitch");   lastPitch  = p; }
    void SetSoundLooping(SoundHandle, bool)    const override { calls.push_back("SetSoundLooping"); }

    bool   IsSoundPlaying  (SoundHandle) const override { return playing; }
    double GetCursorSeconds(SoundHandle) const override { return cursor; }

    // ── Listener (3D) ──
    void SetListenerPosition (float x, float y, float z) override {
        calls.push_back("SetListenerPosition"); lastListenerPosition = { x, y, z };
    }
    void SetListenerDirection(float, float, float) const override { calls.push_back("SetListenerDirection"); }
    void SetListenerWorldUp  (float, float, float) const override { calls.push_back("SetListenerWorldUp"); }

    // ── Voice spatialization ──
    void SetSoundSpatializationEnabled(SoundHandle, bool) const override { calls.push_back("SetSoundSpatializationEnabled"); }
    void SetSoundPosition(SoundHandle, float x, float y, float z) const override {
        calls.push_back("SetSoundPosition"); lastPosition = { x, y, z };
    }
    void SetSoundMinDistance     (SoundHandle, float) const override { calls.push_back("SetSoundMinDistance"); }
    void SetSoundMaxDistance     (SoundHandle, float) const override { calls.push_back("SetSoundMaxDistance"); }
    void SetSoundAttenuationModel(SoundHandle, AttenuationModel) const override { calls.push_back("SetSoundAttenuationModel"); }

    // ── Effect chains ──
    EffectChainHandle CreateEffectChain(SoundHandle, const AudioGraphDesc&, AudioBus) const override {
        calls.push_back("CreateEffectChain");
        return reinterpret_cast<EffectChainHandle>(nextHandle++);
    }
    void SetEffectParam(EffectChainHandle, int, int, float) const override { calls.push_back("SetEffectParam"); }
    void DestroyEffectChain(EffectChainHandle) const noexcept override { calls.push_back("DestroyEffectChain"); }
};

struct FakeVideoBroker : IVideoBroker
{
    mutable std::vector<std::string> calls;
    mutable std::uintptr_t           nextHandle = 1;
    float                            duration   = 10.0f;

    bool Called(const char* name) const {
        return std::find(calls.begin(), calls.end(), name) != calls.end();
    }
    int CountOf(const char* name) const {
        return static_cast<int>(std::count(calls.begin(), calls.end(), name));
    }

    VideoHandle CreateVideo(ResourceVideo*) const override {
        calls.push_back("CreateVideo");
        return reinterpret_cast<VideoHandle>(nextHandle++);
    }
    void  DestroyVideo(VideoHandle) const noexcept override { calls.push_back("DestroyVideo"); }
    void  Start      (VideoHandle) const override { calls.push_back("Start"); }
    void  SetLooping (VideoHandle, bool) const override { calls.push_back("SetLooping"); }
    float GetDuration(VideoHandle) const override { return duration; }
    bool  TryGetFrame(VideoHandle, double, VideoFrame&) const override {
        calls.push_back("TryGetFrame");
        return false;   // no frame available; components must handle this
    }
};

struct FakeResourceLoader : IResourceLoader
{
    mutable std::vector<std::string> calls;
    mutable std::vector<uint32_t>      unloaded;
    mutable std::vector<std::string> imported;

    bool Called(const char* name) const {
        return std::find(calls.begin(), calls.end(), name) != calls.end();
    }

    ResourceBase* CreateResource(const std::string&) override {
        calls.push_back("CreateResource"); return nullptr;
    }
    ResourceBase* CreateResourceFromLibrary(uint32_t, ResourceType, const std::string&,
                                            const std::string&, const std::string&) override {
        calls.push_back("CreateResourceFromLibrary"); return nullptr;
    }
    ResourceMesh* RequestOrCreateSubMeshResource(const std::string&, int32_t) override {
        calls.push_back("RequestOrCreateSubMeshResource"); return nullptr;
    }
    ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(const std::string&, int32_t,
                                                            const std::string&, uint32_t) override {
        calls.push_back("RequestOrCreateSubMeshResourceFromLibrary"); return nullptr;
    }
    bool UnloadResource(uint32_t uid) override {
        calls.push_back("UnloadResource"); unloaded.push_back(uid); return true;
    }
    ResourceMaterial* GetDefaultMaterial() const override {
        calls.push_back("GetDefaultMaterial"); return nullptr;
    }
    bool ImportFile(const std::string& path) override {
        calls.push_back("ImportFile"); imported.push_back(path); return true;
    }
};

struct FakeScriptRegistry : IScriptRegistry
{
    std::vector<std::string> calls;
    int registered = 0;

    bool Called(const char* name) const {
        return std::find(calls.begin(), calls.end(), name) != calls.end();
    }

    void RegisterScriptComponent(CScript*) override   { calls.push_back("Register");   ++registered; }
    void UnregisterScriptComponent(CScript*) override { calls.push_back("Unregister"); --registered; }

    // Always reports "script not found" — there is no script DLL in a unit test.
    // CScript must degrade gracefully (it warns and skips), not crash.
    IScript* CreateScriptInstance(const std::string&) override {
        calls.push_back("CreateScriptInstance"); return nullptr;
    }
};

// Owns one of each fake and exposes a ComponentServices wired to them.
// Hold this BY VALUE in a fixture, declared BEFORE the Scene pointer so it
// outlives the Scene, and pass `&fakes.services` as the Scene ctor's 4th argument.
struct FakeServices
{
    FakeSceneHost      host;
    FakeAudioBroker    audio;
    FakeVideoBroker    video;
    FakeResourceLoader resources;
    FakeScriptRegistry scripts;

    ComponentServices services;

    FakeServices() {
        services.host      = &host;
        services.audio     = &audio;
        services.video     = &video;
        services.resources = &resources;
        services.scripts   = &scripts;
    }
};
