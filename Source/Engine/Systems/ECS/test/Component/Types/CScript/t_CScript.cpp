#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include <Scripting/Internal/IScript.inl>
#include <Scripting/iScriptRegistry.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

// CScript's Awake/Start lifecycle depends on the simulation state at the moment
// the component is built. The order differs between hosts:
//
//   EditorApp : scene deserializes while STOPPED, then the user presses Play.
//   GameApp   : PressPlay fires first (MainGame.cpp starts the sim as soon as the
//               job queue drains), and the scene deserializes into a LIVE sim.
//
// The second order is the one that broke: scripts never received Start().

namespace
{
    // FakeScriptRegistry always reports "script not found", which is right for the
    // other component tests but useless here -- we need a real instance to observe
    // the lifecycle on.
    struct RecordingScript final : public IScript
    {
        static inline int s_awakes = 0;
        static inline int s_starts = 0;
        static inline int s_updates = 0;

        // Animation events arrive through this vtable, exactly as Update does -- the
        // engine already calls outward here every frame.
        static inline std::vector<std::string> s_events;
        static inline std::vector<float>       s_floats;
        static inline std::vector<std::string> s_strings;

        static void Reset()
        {
            s_awakes = 0; s_starts = 0; s_updates = 0;
            s_events.clear(); s_floats.clear(); s_strings.clear();
        }

        void Awake()            override { ++s_awakes; }
        void Start()            override { ++s_starts; }
        void Update(float)      override { ++s_updates; }
        void LateUpdate(float)  override {}
        void OnEnable()         override {}
        void OnDisable()        override {}
        void OnDestroy()        override {}

        void OnAnimationEvent(const char* name, float f, const char* s) override
        {
            s_events.push_back(name ? name : "");
            s_floats.push_back(f);
            s_strings.push_back(s ? s : "");
        }
    };

    struct RecordingRegistry final : public IScriptRegistry
    {
        void RegisterScriptComponent(CScript*)   override {}
        void UnregisterScriptComponent(CScript*) override {}

        IScript* CreateScriptInstance(const std::string&) override
        {
            return new RecordingScript();   // released via IScript::Destroy()
        }
    };

    // The on-disk shape CScript::Serialize produces for a single script, with no
    // saved SCRIPT_FIELD values.
    JsonObject MakeSceneJson(const char* scriptName)
    {
        JsonObject obj;
        JsonArray  scripts;
        scripts.Append(scriptName);
        obj.Set("scripts", std::move(scripts));
        return obj;
    }
}

class t_CScript : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        RecordingScript::Reset();

        // Swap in a registry that hands back real instances.
        fakes.services.scripts = &registry;
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", &fakes.services);
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }

    // Declared before `scene` so both outlive it -- the Scene holds a pointer in.
    FakeServices      fakes;
    RecordingRegistry registry;
    Scene*            scene = nullptr;
};

// Baseline: the EditorApp order. Scene loads while stopped, then Play.
TEST_F(t_CScript, ScriptDeserializedWhileStoppedStartsOnPlay)
{
    fakes.host.playing = false;

    GameObject go = scene->CreateGameObject("Main Camera");
    go.AddComponent<CScript>();
    CScript& cs = go.GetComponent<CScript>();

    cs.Deserialize(MakeSceneJson("CameraMovement"));
    EXPECT_EQ(RecordingScript::s_starts, 0) << "must not start while stopped";

    // PressPlay's per-component call.
    fakes.host.playing = true;
    cs.StartInstances();

    EXPECT_EQ(RecordingScript::s_awakes, 1);
    EXPECT_EQ(RecordingScript::s_starts, 1);
}

// The GameApp order, and the regression. OnStart() runs against an EMPTY
// m_scriptNames (names arrive later, in Deserialize), so StartInstances() marks
// m_started=true having started nothing; the instance created afterwards then
// never receives Awake/Start, and a camera script that gates Update() on state
// set in Start() does nothing for the rest of the session.
TEST_F(t_CScript, ScriptDeserializedWhileSimulationPlayingIsStarted)
{
    fakes.host.playing = true;              // PressPlay already happened

    GameObject go = scene->CreateGameObject("Main Camera");
    go.AddComponent<CScript>();             // OnStart(): 0 names, 0 instances
    CScript& cs = go.GetComponent<CScript>();

    cs.Deserialize(MakeSceneJson("CameraMovement"));

    EXPECT_EQ(RecordingScript::s_awakes, 1) << "instance created into a live sim never woke";
    EXPECT_EQ(RecordingScript::s_starts, 1) << "instance created into a live sim never started";
}

// Whichever path started them, a second start must not double-fire.
TEST_F(t_CScript, StartIsNotFiredTwice)
{
    fakes.host.playing = true;

    GameObject go = scene->CreateGameObject("Main Camera");
    go.AddComponent<CScript>();
    CScript& cs = go.GetComponent<CScript>();

    cs.Deserialize(MakeSceneJson("CameraMovement"));
    cs.StartInstances();                    // PressPlay arriving afterwards

    EXPECT_EQ(RecordingScript::s_starts, 1);
}

// =============================================================================
// Animation event dispatch
// =============================================================================

TEST_F(t_CScript, DispatchAnimationEventReachesEveryInstance)
{
    GameObject go = scene->CreateGameObject("Character");
    go.AddComponent<CScript>();
    CScript& cs = go.GetComponent<CScript>();

    JsonObject obj;
    JsonArray  scripts;
    scripts.Append("AnimatorDemo");
    scripts.Append("ThirdPersonCamera");
    obj.Set("scripts", std::move(scripts));
    cs.Deserialize(obj);

    cs.DispatchAnimationEvent("Footstep", 1.5f, "L");

    // Two instances, so the event is broadcast twice.
    ASSERT_EQ(RecordingScript::s_events.size(), 2u);
    EXPECT_EQ(RecordingScript::s_events[0], "Footstep");
    EXPECT_FLOAT_EQ(RecordingScript::s_floats[0], 1.5f);
    EXPECT_EQ(RecordingScript::s_strings[1], "L");
}

// Null-safety matters because the parameters are BORROWED pointers into a resource;
// a script must never receive a raw nullptr to strcmp against.
TEST_F(t_CScript, DispatchAnimationEventPassesEmptyStringsRatherThanNull)
{
    GameObject go = scene->CreateGameObject("Character");
    go.AddComponent<CScript>();
    CScript& cs = go.GetComponent<CScript>();
    cs.Deserialize(MakeSceneJson("AnimatorDemo"));

    cs.DispatchAnimationEvent(nullptr, 0.0f, nullptr);

    ASSERT_EQ(RecordingScript::s_events.size(), 1u);
    EXPECT_EQ(RecordingScript::s_events[0], "");
    EXPECT_EQ(RecordingScript::s_strings[0], "");
}

TEST_F(t_CScript, DispatchAnimationEventOnAComponentWithNoScriptsDoesNothing)
{
    GameObject go = scene->CreateGameObject("Character");
    go.AddComponent<CScript>();

    go.GetComponent<CScript>().DispatchAnimationEvent("Hit", 0.0f, "");

    EXPECT_TRUE(RecordingScript::s_events.empty());
}
