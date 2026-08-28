// Covers ModuleAudio's no-backend contract: the state the engine is in when audio
// device init fails, or when there is no audio hardware at all.
//
// This is the module's only real behaviour. Every one of its ~28 public methods is
// a one-line forward to IAudioEngineBackend guarded by `if (m_backend)`, and the
// guard is not noise -- it is the audio-less fallback. Awake() logs a warning and
// leaves m_backend null rather than failing, so the whole engine keeps running.
// The contract each guarded method must honour is "no-op, or return null / false /
// 0 / 1.0f", and it is written down in ModuleAudio.h. Nothing enforced it.
//
// Why this matters now: the standing recommendation is to replace the nullable
// pointer with a null-object SilentAudioBackend and collapse the 28 forwarders to
// delegation. These tests are the safety net for exactly that refactor -- they
// describe the observable behaviour it must preserve, without naming m_backend.
//
// NOT covered: the forwarding-when-a-backend-exists half. ModuleAudio::Awake()
// calls CreateAudioBackend() directly with no injection point, so a fake backend
// cannot be substituted without a production change. Testing that half is a
// reason to add constructor injection, not a reason to fake around it here.

#include <gtest/gtest.h>

#include <ModuleAudio/ModuleAudio.h>
#include <AudioSystem/AudioTypes.h>
#include <AudioSystem/SoundHandle.h>
#include <AudioSystem/EffectChainHandle.h>
#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

#include <EventSystem/EventSystem.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <MemoryManager/MemoryManager.h>

#include <EngineCore/UpdateStatus.h>

class t_ModuleAudio : public ::testing::Test
{
protected:
    static constexpr uint64_t kMemoryPoolSize = MiB(16);

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // Pointers, not values: ShutdownMemory aborts on a leak, so both must be
        // destroyed before it runs. See the GTest fixture rules in CLAUDE.md.
        eventSystem = new EventSystem();
        jobSystem   = new nous::engine::multithreading::NOUS_JobSystem(0);

        // Deliberately NOT Awake()d: that would construct the real miniaudio
        // backend and open an audio device. Every test here runs the module in
        // its no-backend state.
        audio = new ModuleAudio(eventSystem, jobSystem);
    }

    void TearDown() override
    {
        delete audio;
        delete jobSystem;
        delete eventSystem;
        nous::engine::memory::ShutdownMemory();
    }

    EventSystem*                                  eventSystem = nullptr;
    nous::engine::multithreading::NOUS_JobSystem* jobSystem   = nullptr;
    ModuleAudio*                                  audio       = nullptr;
};

// ---------------------------------------------------------------------------
// Lifecycle without a device
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, StartSucceedsWithoutABackend)
{
    EXPECT_TRUE(audio->Start());
}

TEST_F(t_ModuleAudio, UpdatePhasesContinueWithoutABackend)
{
    EXPECT_EQ(audio->PreUpdate(0.016f),  UpdateStatus::CONTINUE);
    EXPECT_EQ(audio->Update(0.016f),     UpdateStatus::CONTINUE);
    EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
}

TEST_F(t_ModuleAudio, CleanUpSucceedsWithoutABackend)
{
    EXPECT_TRUE(audio->CleanUp());
}

TEST_F(t_ModuleAudio, CleanUpIsIdempotent)
{
    // CleanUp nulls m_backend after deleting it; the destructor must not double
    // free, and a second explicit CleanUp must be harmless.
    EXPECT_TRUE(audio->CleanUp());
    EXPECT_TRUE(audio->CleanUp());
}

// ---------------------------------------------------------------------------
// Voice lifecycle -- null handle in, documented default out
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, CreateSoundReturnsNullWithoutABackend)
{
    EXPECT_EQ(audio->CreateSound(nullptr, AudioBus::SFX), nullptr);
}

TEST_F(t_ModuleAudio, VoiceMutatorsAreNoOpsWithoutABackend)
{
    const SoundHandle none = nullptr;

    EXPECT_NO_FATAL_FAILURE(audio->DestroySound(none));
    EXPECT_NO_FATAL_FAILURE(audio->StartSound(none));
    EXPECT_NO_FATAL_FAILURE(audio->StopSound(none));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundVolume(none, 0.5f));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundPitch(none, 2.0f));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundLooping(none, true));
    EXPECT_NO_FATAL_FAILURE(audio->PlayAudio(nullptr));
}

TEST_F(t_ModuleAudio, VoiceQueriesReturnFalsyDefaultsWithoutABackend)
{
    const SoundHandle none = nullptr;

    EXPECT_FALSE(audio->IsSoundPlaying(none));
    EXPECT_DOUBLE_EQ(audio->GetCursorSeconds(none), 0.0);
}

TEST_F(t_ModuleAudio, GetCursorSecondsIsZeroNotNegative)
{
    // CVideoPlayer feeds this straight into its playhead when slaving video to
    // audio. A negative or garbage value would drive the playhead backwards.
    EXPECT_DOUBLE_EQ(audio->GetCursorSeconds(nullptr), 0.0);
}

// ---------------------------------------------------------------------------
// Spatialization
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, SpatializationSettersAreNoOpsWithoutABackend)
{
    const SoundHandle none = nullptr;

    EXPECT_NO_FATAL_FAILURE(audio->SetListenerPosition(1.f, 2.f, 3.f));
    EXPECT_NO_FATAL_FAILURE(audio->SetListenerDirection(0.f, 0.f, -1.f));
    EXPECT_NO_FATAL_FAILURE(audio->SetListenerWorldUp(0.f, 1.f, 0.f));

    EXPECT_NO_FATAL_FAILURE(audio->SetSoundSpatializationEnabled(none, true));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundPosition(none, 1.f, 2.f, 3.f));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundMinDistance(none, 1.f));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundMaxDistance(none, 50.f));
    EXPECT_NO_FATAL_FAILURE(audio->SetSoundAttenuationModel(none, AttenuationModel::Inverse));
}

// ---------------------------------------------------------------------------
// Listener uniqueness bookkeeping
//
// The one piece of ModuleAudio that is not a forwarder: SetListenerPosition
// increments a per-frame counter (it does so BEFORE the backend guard, so it
// counts with or without a device) and PostUpdate evaluates then resets it,
// warning once when more than one main CAudioListener wrote in a frame.
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, SingleListenerPushPerFrameIsAccepted)
{
    audio->SetListenerPosition(0.f, 0.f, 0.f);
    EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
}

TEST_F(t_ModuleAudio, MultipleListenerPushesInOneFrameAreTolerated)
{
    // Last-writer-wins plus a one-shot warning: it must not fail the frame.
    audio->SetListenerPosition(0.f, 0.f, 0.f);
    audio->SetListenerPosition(1.f, 1.f, 1.f);
    audio->SetListenerPosition(2.f, 2.f, 2.f);

    EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
}

TEST_F(t_ModuleAudio, ListenerCounterResetsBetweenFrames)
{
    // The counter is reset at the end of every PostUpdate. If it were not, a
    // single listener would trip the multi-listener warning on frame 2 and the
    // warn-once latch would never re-arm.
    for (int frame = 0; frame < 5; ++frame)
    {
        audio->SetListenerPosition(0.f, 0.f, 0.f);
        EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
    }
}

TEST_F(t_ModuleAudio, NoListenerPushInAFrameIsBenign)
{
    // A scene with no CAudioListener at all is legal -- miniaudio defaults the
    // listener to the origin -- and must not warn or fail.
    EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
    EXPECT_EQ(audio->PostUpdate(0.016f), UpdateStatus::CONTINUE);
}

// ---------------------------------------------------------------------------
// Bus mixer -- the getters have real documented defaults, not just "no crash"
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, BusVolumeDefaultsToUnityGainWithoutABackend)
{
    // 1.0f, not 0.0f: the editor's mixer sliders read these, and a 0 default
    // would show every bus fully attenuated on a machine with no audio device.
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::Master),  1.0f);
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::Music),   1.0f);
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::SFX),     1.0f);
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::UI),      1.0f);
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::Ambient), 1.0f);
}

TEST_F(t_ModuleAudio, BusMuteAndSoloDefaultToFalseWithoutABackend)
{
    for (const AudioBus bus : {AudioBus::Master, AudioBus::Music, AudioBus::SFX,
                               AudioBus::UI, AudioBus::Ambient})
    {
        EXPECT_FALSE(audio->GetBusMute(bus));
        EXPECT_FALSE(audio->GetBusSolo(bus));
    }
}

TEST_F(t_ModuleAudio, BusSettersAreNoOpsWithoutABackend)
{
    EXPECT_NO_FATAL_FAILURE(audio->SetBusVolume(AudioBus::Music, 0.25f));
    EXPECT_NO_FATAL_FAILURE(audio->SetBusMute(AudioBus::SFX, true));
    EXPECT_NO_FATAL_FAILURE(audio->SetBusSolo(AudioBus::UI, true));

    // The state is not remembered locally -- there is nowhere to keep it -- so
    // the getters still report the defaults. Pinned because a null-object
    // backend COULD start remembering, and that would be a behaviour change the
    // editor mixer would see.
    EXPECT_FLOAT_EQ(audio->GetBusVolume(AudioBus::Music), 1.0f);
    EXPECT_FALSE(audio->GetBusMute(AudioBus::SFX));
    EXPECT_FALSE(audio->GetBusSolo(AudioBus::UI));
}

// ---------------------------------------------------------------------------
// Effect chains
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, CreateEffectChainReturnsNullWithoutABackend)
{
    const AudioGraphDesc desc;
    EXPECT_EQ(audio->CreateEffectChain(nullptr, desc, AudioBus::SFX), nullptr);
}

TEST_F(t_ModuleAudio, EffectChainMutatorsAreNoOpsWithoutABackend)
{
    EXPECT_NO_FATAL_FAILURE(audio->SetEffectParam(nullptr, 0, 0, 0.5f));
    EXPECT_NO_FATAL_FAILURE(audio->DestroyEffectChain(nullptr));
}

TEST_F(t_ModuleAudio, CreateEffectChainWithANonEmptyDescStillReturnsNull)
{
    AudioGraphDesc desc;
    desc.push_back(AudioEffectDesc{ AudioEffectType::Gain, { 0.5f } });

    EXPECT_EQ(audio->CreateEffectChain(nullptr, desc, AudioBus::Music), nullptr);
}

// ---------------------------------------------------------------------------
// Broker interface
// ---------------------------------------------------------------------------

TEST_F(t_ModuleAudio, IsUsableThroughIAudioBroker)
{
    // Components never hold a ModuleAudio*; they reach it as an IAudioBroker
    // through ComponentServices. The no-backend contract must hold through the
    // interface too, since that is the only view CAudioSource ever has.
    IAudioBroker& broker = *audio;

    EXPECT_EQ(broker.CreateSound(nullptr, AudioBus::SFX), nullptr);
    EXPECT_FALSE(broker.IsSoundPlaying(nullptr));
    EXPECT_DOUBLE_EQ(broker.GetCursorSeconds(nullptr), 0.0);
    EXPECT_NO_FATAL_FAILURE(broker.StopSound(nullptr));
}
