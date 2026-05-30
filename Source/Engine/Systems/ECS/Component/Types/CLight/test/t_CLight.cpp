#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CLight/include/CLight.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

class t_CLight : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }

    Scene* scene = nullptr;
};

// =============================================================================
// Default state
// =============================================================================

TEST_F(t_CLight, DefaultType_IsDirectional)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Directional);
}

TEST_F(t_CLight, DefaultColor_IsWhite)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    const glm::vec3 c = go.GetComponent<CLight>().color;
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 1.0f);
    EXPECT_FLOAT_EQ(c.b, 1.0f);
}

TEST_F(t_CLight, DefaultIntensity_Is1)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().intensity, 1.0f);
}

TEST_F(t_CLight, DefaultRange_Is10)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().range, 10.0f);
}

// =============================================================================
// Field mutation
// =============================================================================

TEST_F(t_CLight, Type_CanBeChangedToPoint)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().type = LightType::Point;
    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Point);
}

TEST_F(t_CLight, Color_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().color = glm::vec3(1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().color.r, 1.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().color.g, 0.0f);
}

TEST_F(t_CLight, Intensity_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().intensity = 2.5f;
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().intensity, 2.5f);
}

TEST_F(t_CLight, Range_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().range = 50.0f;
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().range, 50.0f);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CLight, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_TRUE(go.HasComponent<CLight>());
}

TEST_F(t_CLight, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_EQ(&go.GetComponent<CLight>(), &go.GetComponent<CLight>());
}

TEST_F(t_CLight, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoLight");
    EXPECT_EQ(go.TryGetComponent<CLight>(), nullptr);
}

TEST_F(t_CLight, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.RemoveComponent<CLight>();
    EXPECT_FALSE(go.HasComponent<CLight>());
}

TEST_F(t_CLight, MultipleGameObjects_IndependentCLight)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CLight>();
    b.AddComponent<CLight>();
    b.GetComponent<CLight>().intensity = 5.0f;
    EXPECT_FLOAT_EQ(a.GetComponent<CLight>().intensity, 1.0f);
    EXPECT_FLOAT_EQ(b.GetComponent<CLight>().intensity, 5.0f);
}

// =============================================================================
// Spot light type
// =============================================================================

TEST_F(t_CLight, DefaultInnerAngle_Is15)
{
    GameObject go = scene->CreateGameObject("SpotLight");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().innerAngle, 15.0f);
}

TEST_F(t_CLight, DefaultOuterAngle_Is25)
{
    GameObject go = scene->CreateGameObject("SpotLight");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().outerAngle, 25.0f);
}

TEST_F(t_CLight, Type_CanBeChangedToSpot)
{
    GameObject go = scene->CreateGameObject("SpotLight");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().type = LightType::Spot;
    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Spot);
}

// =============================================================================
// Serialization
// =============================================================================

TEST_F(t_CLight, Spot_Serialize_WritesLightTypeSpot)
{
    GameObject go  = scene->CreateGameObject("SpotLight");
    auto& light    = go.AddComponent<CLight>();
    light.type     = LightType::Spot;
    light.innerAngle = 20.0f;
    light.outerAngle = 35.0f;

    JsonObject val = light.Serialize();

    EXPECT_EQ(val.GetString("lightType"), "Spot");
    EXPECT_FLOAT_EQ(val.GetFloat("innerAngle", 0.0f), 20.0f);
    EXPECT_FLOAT_EQ(val.GetFloat("outerAngle", 0.0f), 35.0f);
}

TEST_F(t_CLight, Spot_Deserialize_RoundTrip_PreservesAngles)
{
    GameObject go  = scene->CreateGameObject("SpotLight");
    auto& light    = go.AddComponent<CLight>();
    light.type     = LightType::Spot;
    light.innerAngle = 12.0f;
    light.outerAngle = 28.0f;

    JsonObject val = light.Serialize();

    go.GetComponent<CLight>().type       = LightType::Directional;
    go.GetComponent<CLight>().innerAngle = 0.0f;
    go.GetComponent<CLight>().outerAngle = 0.0f;
    go.GetComponent<CLight>().Deserialize(val);

    EXPECT_EQ(go.GetComponent<CLight>().type,       LightType::Spot);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().innerAngle, 12.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().outerAngle, 28.0f);
}

TEST_F(t_CLight, NonSpot_Deserialize_AngleFieldsOptional)
{
    JsonObject obj;
    obj.Set("lightType", std::string("Point"));
    obj.Set("colorR",    1.0);
    obj.Set("colorG",    1.0);
    obj.Set("colorB",    1.0);
    obj.Set("intensity", 1.0);
    obj.Set("range",     10.0);

    GameObject go = scene->CreateGameObject("PointLight");
    go.AddComponent<CLight>().Deserialize(obj);

    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Point);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().innerAngle, 15.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().outerAngle, 25.0f);
}
