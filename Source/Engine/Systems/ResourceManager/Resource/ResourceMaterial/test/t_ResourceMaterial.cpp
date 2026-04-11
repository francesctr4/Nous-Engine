#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Core/Globals.h"

// =====================================================
// Tests — ResourceMaterial
// =====================================================

TEST(t_ResourceMaterial, ConstructorSetsTypeMaterialAndUID)
{
    ResourceMaterial mat(7);
    EXPECT_EQ(mat.GetType(), ResourceType::MATERIAL);
    EXPECT_EQ(mat.GetUID(), 7u);
}

TEST(t_ResourceMaterial, SetShaderNullClearsShaderAndUID)
{
    ResourceMaterial mat;
    ResourceShader shader(55);

    mat.SetShader(&shader);
    EXPECT_EQ(mat.shader,    &shader);
    EXPECT_EQ(mat.shaderUID, 55u);

    mat.SetShader(nullptr);
    EXPECT_EQ(mat.shader,    nullptr);
    EXPECT_EQ(mat.shaderUID, INVALID_ID);
}

TEST(t_ResourceMaterial, SetShaderClearsPoolOwnerShader)
{
    ResourceMaterial mat;
    ResourceShader shaderA(1);
    ResourceShader shaderB(2);

    mat.SetShader(&shaderA);
    mat.poolOwnerShader = &shaderA;  // simulate GPU-side assignment

    // Switching shader must clear the stale GPU pool pointer.
    mat.SetShader(&shaderB);
    EXPECT_EQ(mat.poolOwnerShader, nullptr);
    EXPECT_EQ(mat.shaderUID, 2u);
}

TEST(t_ResourceMaterial, UniformValueMakeDefaultProducesCorrectDefaultsForFloatAndInt)
{
    const UniformValue fv = UniformValue::MakeDefault(UniformValueType::Vec4);
    EXPECT_FLOAT_EQ(fv.fdata.x, 1.0f);
    EXPECT_FLOAT_EQ(fv.fdata.y, 1.0f);
    EXPECT_FLOAT_EQ(fv.fdata.z, 1.0f);
    EXPECT_FLOAT_EQ(fv.fdata.w, 1.0f);

    const UniformValue iv = UniformValue::MakeDefault(UniformValueType::IVec4);
    EXPECT_EQ(iv.idata.x, 1);
    EXPECT_EQ(iv.idata.y, 1);
    EXPECT_EQ(iv.idata.z, 1);
    EXPECT_EQ(iv.idata.w, 1);
}
