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

        static void Reset() { s_awakes = 0; s_starts = 0; s_updates = 0; }

        void Awake()            override { ++s_awakes; }
        void Start()            override { ++s_starts; }
        void Update(float)      override { ++s_updates; }
        void LateUpdate(float)  override {}
        void OnEnable()         override {}
        void OnDisable()        override {}
        void OnDestroy()        override {}
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
