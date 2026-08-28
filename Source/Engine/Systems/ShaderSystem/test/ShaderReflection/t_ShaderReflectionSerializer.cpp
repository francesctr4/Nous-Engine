// Covers the reflection <-> JSON round trip. t_ShaderSystem_ShaderReflection next
// door covers ReflectSpirV (producing the reflection); nothing covered persisting
// it, even though every shader load reads a cached .json rather than re-reflecting.
//
// The property that matters is FIELD-BY-FIELD symmetry: a field added to
// ReflectedBinding/Member/Input/Output and wired into Serialize* but not
// Deserialize* silently reads back as zero, and the failure only shows up as a
// wrong pipeline months later. Every test below writes a non-default value into
// every field precisely so a half-wired field cannot pass.

#include <gtest/gtest.h>

#include <ShaderSystem/ShaderReflection/ShaderReflectionSerializer.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace nous::engine::shader_system;

class t_ShaderReflectionSerializer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tempDir = (fs::temp_directory_path() / "nous_t_shaderreflectionserializer").string();
        fs::create_directories(m_tempDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(m_tempDir, ec);
    }

    [[nodiscard]] std::string Path(const std::string& name) const
    {
        return m_tempDir + "/" + name;
    }

    // A member with every field set to a distinct non-default value.
    static ReflectedMember MakeMember()
    {
        ReflectedMember m;
        m.name       = "uModel";
        m.type       = DataType::Mat4;
        m.offset     = 64;
        m.size       = 64;
        m.arrayCount = 2;
        return m;
    }

    static ReflectedBinding MakeBinding()
    {
        ReflectedBinding b;
        b.set       = 1;
        b.binding   = 3;
        b.type      = DescriptorType::CombinedImageSampler;
        b.count     = 4;
        b.name      = "uAlbedo";
        b.stageMask = 0x11;
        b.blockSize = 720;
        b.members.push_back(MakeMember());
        return b;
    }

    static ReflectedPushConstant MakePushConstant()
    {
        ReflectedPushConstant pc;
        pc.name      = "PushBlock";
        pc.offset    = 16;
        pc.size      = 128;
        pc.stageMask = 0x01;
        pc.members.push_back(MakeMember());
        return pc;
    }

    static ReflectedInput MakeInput()
    {
        ReflectedInput vi;
        vi.location   = 2;
        vi.name       = "aTexCoord";
        vi.components = 2;
        vi.bitWidth   = 32;
        vi.sizeBytes  = 8;
        vi.scalarType = ScalarType::Float;
        return vi;
    }

    static ReflectedOutput MakeOutput()
    {
        ReflectedOutput fo;
        fo.location   = 1;
        fo.name       = "outColor";
        fo.components = 4;
        fo.bitWidth   = 32;
        fo.scalarType = ScalarType::Float;
        return fo;
    }

    static PipelineReflectionResult MakeFullReflection()
    {
        PipelineReflectionResult r;
        r.descriptorSets[1].push_back(MakeBinding());
        r.pushConstants.push_back(MakePushConstant());
        r.vertexInputs.push_back(MakeInput());
        r.fragmentOutputs.push_back(MakeOutput());
        return r;
    }

    std::string m_tempDir;
};

// ---------------------------------------------------------------------------
// Failure paths
// ---------------------------------------------------------------------------

TEST_F(t_ShaderReflectionSerializer, DeserializeMissingFileReturnsFalse)
{
    PipelineReflectionResult out;
    EXPECT_FALSE(DeserializeReflection(Path("does_not_exist.json"), out));
}

TEST_F(t_ShaderReflectionSerializer, DeserializeMalformedFileReturnsFalse)
{
    const std::string p = Path("garbage.json");
    {
        FILE* f = fopen(p.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        fputs("{ this is not valid json", f);
        fclose(f);
    }

    PipelineReflectionResult out;
    EXPECT_FALSE(DeserializeReflection(p, out));
}

TEST_F(t_ShaderReflectionSerializer, SerializeCreatesTheFile)
{
    const std::string p = Path("out.json");

    EXPECT_TRUE(SerializeReflection(MakeFullReflection(), p));
    EXPECT_TRUE(fs::exists(p));
}

// ---------------------------------------------------------------------------
// Empty / degenerate reflections
// ---------------------------------------------------------------------------

TEST_F(t_ShaderReflectionSerializer, EmptyReflectionRoundTripsAsEmpty)
{
    const std::string p = Path("empty.json");
    ASSERT_TRUE(SerializeReflection(PipelineReflectionResult{}, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    EXPECT_TRUE(out.descriptorSets.empty());
    EXPECT_TRUE(out.pushConstants.empty());
    EXPECT_TRUE(out.vertexInputs.empty());
    EXPECT_TRUE(out.fragmentOutputs.empty());
}

TEST_F(t_ShaderReflectionSerializer, BindingWithNoMembersRoundTrips)
{
    // Samplers have no members; only UBO/SSBO bindings do.
    PipelineReflectionResult in;
    ReflectedBinding sampler;
    sampler.set     = 1;
    sampler.binding = 0;
    sampler.type    = DescriptorType::Sampler;
    sampler.name    = "uSampler";
    in.descriptorSets[1].push_back(sampler);

    const std::string p = Path("sampler.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.descriptorSets.count(1u), 1u);
    ASSERT_EQ(out.descriptorSets.at(1).size(), 1u);
    EXPECT_TRUE(out.descriptorSets.at(1)[0].members.empty());
    EXPECT_EQ(out.descriptorSets.at(1)[0].name, "uSampler");
}

// ---------------------------------------------------------------------------
// Field-by-field symmetry
// ---------------------------------------------------------------------------

TEST_F(t_ShaderReflectionSerializer, BindingPreservesEveryField)
{
    PipelineReflectionResult in;
    in.descriptorSets[1].push_back(MakeBinding());

    const std::string p = Path("binding.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.descriptorSets.count(1u), 1u);
    ASSERT_EQ(out.descriptorSets.at(1).size(), 1u);
    const ReflectedBinding& b = out.descriptorSets.at(1)[0];

    EXPECT_EQ(b.set,       1u);
    EXPECT_EQ(b.binding,   3u);
    EXPECT_EQ(b.type,      DescriptorType::CombinedImageSampler);
    EXPECT_EQ(b.count,     4u);
    EXPECT_EQ(b.name,      "uAlbedo");
    EXPECT_EQ(b.stageMask, 0x11u);
    EXPECT_EQ(b.blockSize, 720u);
}

TEST_F(t_ShaderReflectionSerializer, MemberPreservesEveryField)
{
    PipelineReflectionResult in;
    in.descriptorSets[0].push_back(MakeBinding());

    const std::string p = Path("member.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.descriptorSets.at(0).size(), 1u);
    ASSERT_EQ(out.descriptorSets.at(0)[0].members.size(), 1u);
    const ReflectedMember& m = out.descriptorSets.at(0)[0].members[0];

    EXPECT_EQ(m.name,       "uModel");
    EXPECT_EQ(m.type,       DataType::Mat4);
    EXPECT_EQ(m.offset,     64u);
    EXPECT_EQ(m.size,       64u);
    EXPECT_EQ(m.arrayCount, 2u);
}

TEST_F(t_ShaderReflectionSerializer, PushConstantPreservesEveryFieldIncludingMembers)
{
    PipelineReflectionResult in;
    in.pushConstants.push_back(MakePushConstant());

    const std::string p = Path("pc.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.pushConstants.size(), 1u);
    const ReflectedPushConstant& pc = out.pushConstants[0];

    EXPECT_EQ(pc.name,      "PushBlock");
    EXPECT_EQ(pc.offset,    16u);
    EXPECT_EQ(pc.size,      128u);
    EXPECT_EQ(pc.stageMask, 0x01u);
    ASSERT_EQ(pc.members.size(), 1u);
    EXPECT_EQ(pc.members[0].name, "uModel");
}

TEST_F(t_ShaderReflectionSerializer, VertexInputPreservesEveryField)
{
    PipelineReflectionResult in;
    in.vertexInputs.push_back(MakeInput());

    const std::string p = Path("vi.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.vertexInputs.size(), 1u);
    const ReflectedInput& vi = out.vertexInputs[0];

    EXPECT_EQ(vi.location,   2u);
    EXPECT_EQ(vi.name,       "aTexCoord");
    EXPECT_EQ(vi.components, 2);
    EXPECT_EQ(vi.bitWidth,   32);
    EXPECT_EQ(vi.sizeBytes,  8u);
    EXPECT_EQ(vi.scalarType, ScalarType::Float);
    // The derived accessor must still agree after a round trip.
    EXPECT_EQ(vi.ToDataType(), DataType::Vec2);
}

TEST_F(t_ShaderReflectionSerializer, FragmentOutputPreservesEveryField)
{
    PipelineReflectionResult in;
    in.fragmentOutputs.push_back(MakeOutput());

    const std::string p = Path("fo.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.fragmentOutputs.size(), 1u);
    const ReflectedOutput& fo = out.fragmentOutputs[0];

    EXPECT_EQ(fo.location,   1u);
    EXPECT_EQ(fo.name,       "outColor");
    EXPECT_EQ(fo.components, 4);
    EXPECT_EQ(fo.bitWidth,   32);
    EXPECT_EQ(fo.scalarType, ScalarType::Float);
}

// ---------------------------------------------------------------------------
// Structure: several sets, several bindings, ordering
// ---------------------------------------------------------------------------

TEST_F(t_ShaderReflectionSerializer, MultipleDescriptorSetsAreKeptSeparate)
{
    // The engine's whole convention rests on this: set 0 is global, set 1 is
    // per-instance. Collapsing them on load would bind the wrong descriptors.
    PipelineReflectionResult in;

    ReflectedBinding global;
    global.set  = 0;
    global.name = "GlobalUBO";
    global.type = DescriptorType::UniformBuffer;
    in.descriptorSets[0].push_back(global);

    ReflectedBinding instance;
    instance.set  = 1;
    instance.name = "InstanceUBO";
    instance.type = DescriptorType::UniformBuffer;
    in.descriptorSets[1].push_back(instance);

    const std::string p = Path("sets.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.descriptorSets.size(), 2u);
    ASSERT_EQ(out.descriptorSets.count(0u), 1u);
    ASSERT_EQ(out.descriptorSets.count(1u), 1u);
    EXPECT_EQ(out.descriptorSets.at(0)[0].name, "GlobalUBO");
    EXPECT_EQ(out.descriptorSets.at(1)[0].name, "InstanceUBO");
}

TEST_F(t_ShaderReflectionSerializer, BindingOrderWithinASetIsPreserved)
{
    PipelineReflectionResult in;
    for (uint32_t i = 0; i < 3; ++i)
    {
        ReflectedBinding b;
        b.set     = 0;
        b.binding = i;
        b.name    = "b" + std::to_string(i);
        in.descriptorSets[0].push_back(b);
    }

    const std::string p = Path("order.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    ASSERT_EQ(out.descriptorSets.at(0).size(), 3u);
    EXPECT_EQ(out.descriptorSets.at(0)[0].name, "b0");
    EXPECT_EQ(out.descriptorSets.at(0)[1].name, "b1");
    EXPECT_EQ(out.descriptorSets.at(0)[2].name, "b2");
}

TEST_F(t_ShaderReflectionSerializer, MultipleMembersKeepTheirOrder)
{
    // Members are read back by index and matched to offsets; reordering them
    // would silently shift every uniform in the block.
    PipelineReflectionResult in;
    ReflectedBinding b;
    b.set = 0;
    for (uint32_t i = 0; i < 3; ++i)
    {
        ReflectedMember m;
        m.name   = "m" + std::to_string(i);
        m.offset = i * 16;
        m.type   = DataType::Vec4;
        b.members.push_back(m);
    }
    in.descriptorSets[0].push_back(b);

    const std::string p = Path("members.json");
    ASSERT_TRUE(SerializeReflection(in, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    const auto& members = out.descriptorSets.at(0)[0].members;
    ASSERT_EQ(members.size(), 3u);
    EXPECT_EQ(members[0].name, "m0");   EXPECT_EQ(members[0].offset, 0u);
    EXPECT_EQ(members[1].name, "m1");   EXPECT_EQ(members[1].offset, 16u);
    EXPECT_EQ(members[2].name, "m2");   EXPECT_EQ(members[2].offset, 32u);
}

TEST_F(t_ShaderReflectionSerializer, FullReflectionRoundTripsInOnePass)
{
    const std::string p = Path("full.json");
    ASSERT_TRUE(SerializeReflection(MakeFullReflection(), p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));

    EXPECT_EQ(out.descriptorSets.size(),  1u);
    EXPECT_EQ(out.pushConstants.size(),   1u);
    EXPECT_EQ(out.vertexInputs.size(),    1u);
    EXPECT_EQ(out.fragmentOutputs.size(), 1u);
}

TEST_F(t_ShaderReflectionSerializer, SerializingTwiceOverwritesRatherThanAppends)
{
    const std::string p = Path("twice.json");

    ASSERT_TRUE(SerializeReflection(MakeFullReflection(), p));
    ASSERT_TRUE(SerializeReflection(PipelineReflectionResult{}, p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));
    EXPECT_TRUE(out.descriptorSets.empty());
    EXPECT_TRUE(out.vertexInputs.empty());
}

TEST_F(t_ShaderReflectionSerializer, DeserializeAppendsIntoANonEmptyResult)
{
    // Documented, not endorsed: DeserializeReflection does not clear its out
    // parameter, so vectors accumulate across calls and descriptorSets[i] is
    // appended to. Every current caller passes a fresh result. Pinned so a
    // caller that reuses one gets a failing test rather than duplicate bindings.
    const std::string p = Path("append.json");
    ASSERT_TRUE(SerializeReflection(MakeFullReflection(), p));

    PipelineReflectionResult out;
    ASSERT_TRUE(DeserializeReflection(p, out));
    ASSERT_TRUE(DeserializeReflection(p, out));

    EXPECT_EQ(out.vertexInputs.size(), 2u);
    EXPECT_EQ(out.descriptorSets.at(1).size(), 2u);
}
