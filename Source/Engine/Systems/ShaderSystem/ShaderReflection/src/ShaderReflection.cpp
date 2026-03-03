#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <cstdint>

static DescriptorType ToDescriptorType(SpvReflectDescriptorType t)
{
    switch (t)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return DescriptorType::UniformBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return DescriptorType::StorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return DescriptorType::CombinedImageSampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return DescriptorType::SampledImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                return DescriptorType::Sampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return DescriptorType::StorageImage;
        default:                                                 return DescriptorType::Unknown;
    }
}

// NOTE: Your engine already has ShaderStage (used in tests).
// We map it to our own bitmask so that:
// - Vertex stageMask != Fragment stageMask
// - Later you can OR them when merging pipeline interface.
static uint32_t ToStageMask(ShaderStage s)
{
    switch (s)
    {
        case ShaderStage::Vertex:   return 1u << 0;
        case ShaderStage::Fragment: return 1u << 1;
        // Add more if your engine supports them:
        // case ShaderStage::Compute:  return 1u << 2;
        // case ShaderStage::Geometry: return 1u << 3;
        // case ShaderStage::TessControl: return 1u << 4;
        // case ShaderStage::TessEvaluation: return 1u << 5;
        default: return 0;
    }
}

static ScalarType ToScalarType(const SpvReflectTypeDescription* td)
{
    if (!td) return ScalarType::Unknown;

    const uint32_t f = td->type_flags;

    if (f & SPV_REFLECT_TYPE_FLAG_BOOL)  return ScalarType::Bool;
    if (f & SPV_REFLECT_TYPE_FLAG_FLOAT) return ScalarType::Float;
    if (f & SPV_REFLECT_TYPE_FLAG_INT)
    {
        // signedness: 1 = signed, 0 = unsigned
        return td->traits.numeric.scalar.signedness ? ScalarType::Int : ScalarType::UInt;
    }

    return ScalarType::Unknown;
}

static DataType ToMemberDataType(const SpvReflectTypeDescription* td)
{
    if (!td) return DataType::Unknown;

    const uint32_t f = td->type_flags;

    // Matrix?
    if (f & SPV_REFLECT_TYPE_FLAG_MATRIX)
    {
        // Most common for UBO/PC is float matrices.
        // We only store Mat2/3/4 (square). If not square, pick by columns as fallback.
        const uint32_t cols = td->traits.numeric.matrix.column_count;
        const uint32_t rows = td->traits.numeric.matrix.row_count;
        const uint32_t n = (cols == rows) ? cols : cols;

        if (n == 2) return DataType::Mat2;
        if (n == 3) return DataType::Mat3;
        if (n == 4) return DataType::Mat4;
        return DataType::Unknown;
    }

    // Vector?
    if (f & SPV_REFLECT_TYPE_FLAG_VECTOR)
    {
        const uint32_t n = td->traits.numeric.vector.component_count;
        const bool isFloat = (f & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0;
        const bool isInt   = (f & SPV_REFLECT_TYPE_FLAG_INT)   != 0;
        const bool isSigned = td->traits.numeric.scalar.signedness != 0;

        if (isFloat)
        {
            if (n == 2) return DataType::Vec2;
            if (n == 3) return DataType::Vec3;
            if (n == 4) return DataType::Vec4;
        }
        else if (isInt)
        {
            if (isSigned)
            {
                if (n == 2) return DataType::IVec2;
                if (n == 3) return DataType::IVec3;
                if (n == 4) return DataType::IVec4;
            }
            else
            {
                if (n == 2) return DataType::UVec2;
                if (n == 3) return DataType::UVec3;
                if (n == 4) return DataType::UVec4;
            }
        }

        return DataType::Unknown;
    }

    // Scalar
    if (f & SPV_REFLECT_TYPE_FLAG_BOOL)  return DataType::Bool;
    if (f & SPV_REFLECT_TYPE_FLAG_FLOAT) return DataType::Float;
    if (f & SPV_REFLECT_TYPE_FLAG_INT)
        return td->traits.numeric.scalar.signedness ? DataType::Int : DataType::UInt;

    return DataType::Unknown;
}

static uint32_t GetArrayCount(const SpvReflectBlockVariable& m)
{
    // For non-array, return 1.
    if (m.array.dims_count == 0)
        return 1;

    // For fixed arrays, dims[0] is the element count.
    // If it’s 0 (runtime array), keep 0 so you can detect it.
    return m.array.dims[0];
}

ShaderReflectionResult NOUS_ShaderSystem::ReflectSpirV(const ShaderSource& source)
{
    ShaderReflectionResult out{};
    out.success = false;

    if (source.spirvBinary.empty())
    {
        out.errorMessage = "ReflectSpirv: SPIR-V is empty.";
        return out;
    }

    const uint32_t stageMask = ToStageMask(source.stage);
    if (stageMask == 0)
    {
        out.errorMessage = "ReflectSpirv: Unsupported/unknown ShaderStage for stageMask mapping.";
        return out;
    }

    SpvReflectShaderModule module{};
    SpvReflectResult r = spvReflectCreateShaderModule(
        source.spirvBinary.size() * sizeof(uint32_t),
        source.spirvBinary.data(),
        &module
    );

    if (r != SPV_REFLECT_RESULT_SUCCESS)
    {
        out.errorMessage = "ReflectSpirv: Failed to create reflection module.";
        return out;
    }

    // --- Descriptor bindings ---
    uint32_t bindingCount = 0;
    spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);

    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());

    out.bindings.reserve(bindingCount);
    for (SpvReflectDescriptorBinding* b : bindings)
    {
        if (!b) continue;

        ReflectedBinding rb{};
        rb.set      = b->set;
        rb.binding  = b->binding;
        rb.type     = ToDescriptorType(b->descriptor_type);
        rb.count    = b->count;
        rb.name     = (b->name ? b->name : "");

        // Fill new fields:
        rb.stageMask = stageMask;

        // blockSize only makes sense for UBO/SSBO (for images/samplers keep 0)
        rb.blockSize = 0;
        if (rb.type == DescriptorType::UniformBuffer || rb.type == DescriptorType::StorageBuffer)
        {
            // In spirv-reflect, b->block describes the interface block for UBO/SSBO.
            // size is in bytes.
            rb.blockSize = b->block.size;

            rb.members.reserve(b->block.member_count);
            for (uint32_t i = 0; i < b->block.member_count; ++i)
            {
                const SpvReflectBlockVariable& m = b->block.members[i];

                ReflectedMember rm{};
                rm.name       = m.name ? m.name : "";
                rm.offset     = m.offset;
                rm.size       = m.size;
                rm.arrayCount = (m.array.dims_count == 0) ? 1 : m.array.dims[0];
                rm.type       = ToMemberDataType(m.type_description);

                rb.members.push_back(std::move(rm));
            }
        }

        out.bindings.push_back(std::move(rb));
    }

    // --- Push constants ---
    uint32_t pcCount = 0;
    spvReflectEnumeratePushConstantBlocks(&module, &pcCount, nullptr);

    std::vector<SpvReflectBlockVariable*> pcs(pcCount);
    spvReflectEnumeratePushConstantBlocks(&module, &pcCount, pcs.data());

    out.pushConstants.reserve(pcCount);
    for (SpvReflectBlockVariable* pc : pcs)
    {
        if (!pc) continue;

        ReflectedPushConstant rpc{};
        rpc.offset    = pc->offset;
        rpc.size      = pc->size;
        rpc.name      = (pc->name ? pc->name : "");
        rpc.stageMask = stageMask;

        rpc.members.reserve(pc->member_count);
        for (uint32_t i = 0; i < pc->member_count; ++i)
        {
            const SpvReflectBlockVariable& m = pc->members[i];
            ReflectedMember rm{};
            rm.name       = m.name ? m.name : "";
            rm.offset     = m.offset;
            rm.size       = m.size;
            rm.arrayCount = GetArrayCount(m);
            rm.type       = ToMemberDataType(m.type_description);

            rpc.members.push_back(std::move(rm));
        }

        out.pushConstants.push_back(std::move(rpc));
    }

    // --- Vertex inputs (meaningful mostly for Vertex stage) ---
    // We'll enumerate for all stages but you can choose to only store for vertex stage.
    uint32_t inputCount = 0;
    spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);

    std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
    spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());

    if (source.stage == ShaderStage::Vertex)
    {
        out.vertexInputs.reserve(inputCount);

        for (SpvReflectInterfaceVariable* v : inputs)
        {
            if (!v) continue;
            if (v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                continue; // Skip gl_VertexIndex, gl_InstanceIndex, etc.

            ReflectedInput in{};
            in.location = v->location;
            in.name     = (v->name ? v->name : "");

            // Components (vecN). For scalar, component_count can be 0.
            uint32_t comps = 1;
            if (v->type_description)
            {
                const uint32_t tf = v->type_description->type_flags;
                if (tf & SPV_REFLECT_TYPE_FLAG_VECTOR)
                    comps = v->type_description->traits.numeric.vector.component_count;
            }
            in.components = static_cast<uint8_t>(comps);

            // Bit width
            in.bitWidth = static_cast<uint8_t>(v->numeric.scalar.width);
            in.sizeBytes = static_cast<uint32_t>(in.components) * (static_cast<uint32_t>(in.bitWidth) / 8u);

            // Scalar kind (float/int/uint/bool)
            in.scalarType = ToScalarType(v->type_description);

            // Binding index: reflection doesn’t know your vertex buffer slot. Default to 0.
            // Your mesh/vertex-layout system can override this later.
            in.binding = 0;

            out.vertexInputs.push_back(std::move(in));
        }
    }

    spvReflectDestroyShaderModule(&module);

    out.success = true;
    return out;
}
