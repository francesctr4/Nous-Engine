#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"

#include <spirv_reflect.h>

static DescriptorType ToDescriptorType(SpvReflectDescriptorType t)
{
    switch (t)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return DescriptorType::UniformBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return DescriptorType::StorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return DescriptorType::CombinedImageSampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return DescriptorType::SampledImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:               return DescriptorType::Sampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return DescriptorType::StorageImage;
        default:                                                return DescriptorType::Unknown;
    }
}

ShaderReflectionResult NOUS_ShaderSystem::ReflectSpirV(const ShaderSource &source)
{
    ShaderReflectionResult out{};
    out.success = false;

    if (source.spirvBinary.empty())
    {
        out.errorMessage = "ReflectSpirv: SPIR-V is empty.";
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
        ReflectedBinding rb{};
        rb.set = b->set;
        rb.binding = b->binding;
        rb.type = ToDescriptorType(b->descriptor_type);
        rb.count = b->count;
        rb.name = (b->name ? b->name : "");
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
        ReflectedPushConstant rpc{};
        rpc.offset = pc->offset;
        rpc.size = pc->size;
        rpc.name = (pc->name ? pc->name : "");

        rpc.members.reserve(pc->member_count);
        for (uint32_t i = 0; i < pc->member_count; ++i)
        {
            const SpvReflectBlockVariable& m = pc->members[i];
            ReflectedMember rm{};
            rm.name   = m.name ? m.name : "";
            rm.offset = m.offset;
            rm.size   = m.size;
            rpc.members.push_back(std::move(rm));
        }

        out.pushConstants.push_back(rpc);
    }

    // --- Vertex inputs (only meaningful for Vertex stage) ---
    // SPIRV-Reflect can enumerate input variables, but you should filter built-ins.
    uint32_t inputCount = 0;
    spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);

    std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
    spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());

    for (SpvReflectInterfaceVariable* v : inputs)
    {
        if (!v) continue;

        if (v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
            continue; // Skip gl_VertexIndex etc.

        ReflectedInput in{};
        in.location = v->location;
        in.name     = (v->name ? v->name : "");

        uint32_t comps = v->numeric.vector.component_count;
        if (comps == 0)
            comps = 1; // escalar
        in.components = static_cast<uint8_t>(comps);

        in.bitWidth = static_cast<uint8_t>(v->numeric.scalar.width);
        in.sizeBytes = in.components * (in.bitWidth / 8);

        out.vertexInputs.push_back(std::move(in));
    }

    spvReflectDestroyShaderModule(&module);

    out.success = true;
    return out;
}
