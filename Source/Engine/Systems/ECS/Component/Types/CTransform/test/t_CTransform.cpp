#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

class t_CTransform : public ::testing::Test
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
// Default values
// =============================================================================

TEST_F(t_CTransform, DefaultPosition_IsOrigin)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    EXPECT_FLOAT_EQ(t.position.x, 0.f);
    EXPECT_FLOAT_EQ(t.position.y, 0.f);
    EXPECT_FLOAT_EQ(t.position.z, 0.f);
}

TEST_F(t_CTransform, DefaultScale_IsOne)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    EXPECT_FLOAT_EQ(t.scale.x, 1.f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.f);
}

TEST_F(t_CTransform, DefaultOrientation_IsIdentityQuaternion)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    EXPECT_FLOAT_EQ(t.orientation.w, 1.f);
    EXPECT_FLOAT_EQ(t.orientation.x, 0.f);
    EXPECT_FLOAT_EQ(t.orientation.y, 0.f);
    EXPECT_FLOAT_EQ(t.orientation.z, 0.f);
}

// =============================================================================
// SetEulerRotation / GetEulerAngles
// =============================================================================

TEST_F(t_CTransform, SetEulerRotation_UpdatesOrientation)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    t.SetEulerRotation(glm::vec3(0.f, 90.f, 0.f));

    // A 90° Y rotation gives ~0.707 w and ~0.707 y
    EXPECT_NEAR(t.orientation.w, 0.707f, 0.002f);
    EXPECT_NEAR(t.orientation.y, 0.707f, 0.002f);
}

TEST_F(t_CTransform, SetEulerRotation_SyncsEulerHint)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    t.SetEulerRotation(glm::vec3(45.f, 0.f, 0.f));
    EXPECT_NEAR(t.eulerHint.x, 45.f, 0.1f);
}

TEST_F(t_CTransform, GetEulerAngles_RoundTrip)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    const glm::vec3 input(30.f, 45.f, 0.f);
    t.SetEulerRotation(input);
    const glm::vec3 recovered = t.GetEulerAngles();
    EXPECT_NEAR(recovered.x, input.x, 0.5f);
    EXPECT_NEAR(recovered.y, input.y, 0.5f);
}

// =============================================================================
// UpdateMatrix
// =============================================================================

TEST_F(t_CTransform, UpdateMatrix_TranslatesCorrectly)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    t.position = glm::vec3(5.f, 0.f, 0.f);
    t.UpdateMatrix();

    // Column 3 of the model matrix is the translation column.
    EXPECT_NEAR(t.worldMatrix[3][0], 5.f, 0.001f);
}

TEST_F(t_CTransform, UpdateMatrix_ScalesCorrectly)
{
    GameObject go = scene->CreateGameObject("T");
    auto& t = go.GetComponent<CTransform>();
    t.scale = glm::vec3(2.f, 2.f, 2.f);
    t.UpdateMatrix();

    // Scale factor appears on the diagonal.
    EXPECT_NEAR(t.worldMatrix[0][0], 2.f, 0.001f);
    EXPECT_NEAR(t.worldMatrix[1][1], 2.f, 0.001f);
    EXPECT_NEAR(t.worldMatrix[2][2], 2.f, 0.001f);
}

// =============================================================================
// Parent-child world matrix propagation
// =============================================================================

TEST_F(t_CTransform, WorldMatrix_PropagatesParentTranslation)
{
    GameObject parent = scene->CreateGameObject("Parent");
    GameObject child  = scene->CreateGameObject("Child", &parent);

    auto& pt = parent.GetComponent<CTransform>();
    auto& ct = child.GetComponent<CTransform>();

    pt.position = glm::vec3(10.f, 0.f, 0.f);
    ct.position = glm::vec3(5.f, 0.f, 0.f);

    scene->UpdateWorldMatrices();

    // Child world position must be parent + local = 15
    EXPECT_NEAR(ct.worldMatrix[3][0], 15.f, 0.01f);
}
