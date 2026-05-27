#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceShader/include/ResourceShader.h"
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

TEST(t_ResourceMaterial, UniformValueMakeDefault_SetsTypeField)
{
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::Float).type,  UniformValueType::Float);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::Vec2).type,   UniformValueType::Vec2);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::Vec3).type,   UniformValueType::Vec3);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::Vec4).type,   UniformValueType::Vec4);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::Int).type,    UniformValueType::Int);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::IVec2).type,  UniformValueType::IVec2);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::IVec3).type,  UniformValueType::IVec3);
    EXPECT_EQ(UniformValue::MakeDefault(UniformValueType::IVec4).type,  UniformValueType::IVec4);
}

TEST(t_ResourceMaterial, UniformValueMakeDefault_RemainingFloatTypes_DefaultToOne)
{
    // MakeDefault stores glm::vec4(1.0f) for all float-based types.
    // Vec4 is covered by the existing test above; this covers Float, Vec2, Vec3.
    const UniformValue flt = UniformValue::MakeDefault(UniformValueType::Float);
    EXPECT_FLOAT_EQ(flt.fdata.x, 1.0f);

    const UniformValue v2 = UniformValue::MakeDefault(UniformValueType::Vec2);
    EXPECT_FLOAT_EQ(v2.fdata.x, 1.0f);
    EXPECT_FLOAT_EQ(v2.fdata.y, 1.0f);

    const UniformValue v3 = UniformValue::MakeDefault(UniformValueType::Vec3);
    EXPECT_FLOAT_EQ(v3.fdata.x, 1.0f);
    EXPECT_FLOAT_EQ(v3.fdata.y, 1.0f);
    EXPECT_FLOAT_EQ(v3.fdata.z, 1.0f);
}

TEST(t_ResourceMaterial, UniformValueMakeDefault_RemainingIntTypes_DefaultToOne)
{
    // MakeDefault stores glm::ivec4(1) for all int-based types.
    // IVec4 is covered by the existing test above; this covers Int, IVec2, IVec3.
    const UniformValue i1 = UniformValue::MakeDefault(UniformValueType::Int);
    EXPECT_EQ(i1.idata.x, 1);

    const UniformValue i2 = UniformValue::MakeDefault(UniformValueType::IVec2);
    EXPECT_EQ(i2.idata.x, 1);
    EXPECT_EQ(i2.idata.y, 1);

    const UniformValue i3 = UniformValue::MakeDefault(UniformValueType::IVec3);
    EXPECT_EQ(i3.idata.x, 1);
    EXPECT_EQ(i3.idata.y, 1);
    EXPECT_EQ(i3.idata.z, 1);
}

// =============================================================================
// UniformValueIsInt
// =============================================================================

TEST(t_ResourceMaterial, UniformValueIsInt_FloatTypes_ReturnFalse)
{
    EXPECT_FALSE(UniformValueIsInt(UniformValueType::Float));
    EXPECT_FALSE(UniformValueIsInt(UniformValueType::Vec2));
    EXPECT_FALSE(UniformValueIsInt(UniformValueType::Vec3));
    EXPECT_FALSE(UniformValueIsInt(UniformValueType::Vec4));
}

TEST(t_ResourceMaterial, UniformValueIsInt_IntTypes_ReturnTrue)
{
    EXPECT_TRUE(UniformValueIsInt(UniformValueType::Int));
    EXPECT_TRUE(UniformValueIsInt(UniformValueType::IVec2));
    EXPECT_TRUE(UniformValueIsInt(UniformValueType::IVec3));
    EXPECT_TRUE(UniformValueIsInt(UniformValueType::IVec4));
}

// =============================================================================
// UniformValueComponentCount
// =============================================================================

TEST(t_ResourceMaterial, UniformValueComponentCount_Scalars_Return1)
{
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::Float), 1u);
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::Int),   1u);
}

TEST(t_ResourceMaterial, UniformValueComponentCount_Vec2_Returns2)
{
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::Vec2),  2u);
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::IVec2), 2u);
}

TEST(t_ResourceMaterial, UniformValueComponentCount_Vec3_Returns3)
{
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::Vec3),  3u);
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::IVec3), 3u);
}

TEST(t_ResourceMaterial, UniformValueComponentCount_Vec4_Returns4)
{
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::Vec4),  4u);
    EXPECT_EQ(UniformValueComponentCount(UniformValueType::IVec4), 4u);
}

// =============================================================================
// DataTypeToUniformValueType
// =============================================================================

TEST(t_ResourceMaterial, DataTypeToUniformValueType_MapsFloatTypes)
{
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Float), UniformValueType::Float);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Vec2),  UniformValueType::Vec2);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Vec3),  UniformValueType::Vec3);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Vec4),  UniformValueType::Vec4);
}

TEST(t_ResourceMaterial, DataTypeToUniformValueType_MapsIntTypes)
{
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Int),   UniformValueType::Int);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::IVec2), UniformValueType::IVec2);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::IVec3), UniformValueType::IVec3);
    EXPECT_EQ(DataTypeToUniformValueType(DataType::IVec4), UniformValueType::IVec4);
}

TEST(t_ResourceMaterial, DataTypeToUniformValueType_UnknownDefaultsToVec4)
{
    EXPECT_EQ(DataTypeToUniformValueType(DataType::Unknown), UniformValueType::Vec4);
}

// =============================================================================
// ResourceMaterial default state
// =============================================================================

TEST(t_ResourceMaterial, DefaultTextureMaps_IsEmpty)
{
    ResourceMaterial mat;
    EXPECT_TRUE(mat.textureMaps.empty());
}

TEST(t_ResourceMaterial, DefaultUniformValues_IsEmpty)
{
    ResourceMaterial mat;
    EXPECT_TRUE(mat.uniformValues.empty());
}
