#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCMaterial/include/InspectorCMaterial.h"

#include <Logger/Logger.h>
#include <ModuleResourceManager/ModuleResourceManager.h>
#include <RendererFrontend/RendererFrontend.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ECS/GameObject.h>
#include <ResourceManager/Types/ResourceMaterial/ImporterMaterial.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

#include <filesystem>
#include <imgui.h>
#include <format>

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawDefaultMaterial(const GameObject* go, CMaterial& mat,
                                   ModuleResourceManager* rm, const std::string& saveDir)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.33f, 1.0f));
    ImGui::TextUnformatted("Using Default Material (read-only).");
    ImGui::TextUnformatted(
        "Create a material asset to assign textures / shaders that persist with the scene.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (ImGui::Button("Create Material Asset"))
    {
        // Build a safe filename from the GameObject name.
        std::string safeName = go->GetName();
        if (safeName.empty()) safeName = "Material";
        for (char& c : safeName)
        {
            // Replace characters invalid on Windows file systems with '_'.
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|' || c == ' ')
                c = '_';
        }

        const std::string dir = saveDir + "/";
        std::string assetPath = dir + safeName + "_material.nmat";

        // Ensure uniqueness: append _1, _2, ... until we find a free path.
        int suffix = 1;

        while (std::filesystem::exists(assetPath))
        {
            assetPath = std::format("{}{}_material_{}.nmat",
                                    dir,
                                    safeName,
                                    suffix);

            ++suffix;

            if (suffix > 1000) break; // safety guard
        }

        if (!ImporterMaterial::CreateNewMaterialFile(assetPath))
        {
            NOUS_ERROR("InspectorWindow — failed to write new material file '%s'.", assetPath.c_str());
        }
        else if (!rm->ImportFile(assetPath))
        {
            NOUS_ERROR("InspectorWindow — failed to import new material file '%s'.", assetPath.c_str());
        }
        else
        {
            ResourceBase* r = rm->CreateResource(assetPath);
            if (r)
                mat.material = down_cast<ResourceMaterial*>(r);
            else
                NOUS_ERROR("InspectorWindow — failed to load new material '%s'.", assetPath.c_str());
        }
    }
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawShaderSelector(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm)
{
    ImGui::SeparatorText("SHADER");

    if (const std::string label = mat.material->shader
                                  ? mat.material->shader->GetName()
                                  : "Default (MaterialShader)";
    !ImGui::BeginCombo("Shader", label.c_str()))
        return;

    // Default entry — clears any custom shader assignment.
    DrawDefaultShaderOption(rendererFrontend, mat, rm);

    // Scan Assets/Shaders/ for .glsl files.
    LoadAndDisplayShaderOptions(rendererFrontend, mat, rm);

    ImGui::EndCombo();
}

void DrawDefaultShaderOption(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm)
{
    const bool isDefault = (mat.material->shader == nullptr);

    if (ImGui::Selectable("Default (MaterialShader)", isDefault))
    {
        if (isDefault)
            return;

        rm->UnloadResource(mat.material->shader->GetUID());

        rendererFrontend->RequestMaterialShaderChange(mat.material, nullptr);

        mat.material->uniformValues.clear();
    }

    if (isDefault)
        ImGui::SetItemDefaultFocus();
}

void LoadAndDisplayShaderOptions(RendererFrontend* rendererFrontend, CMaterial& mat, ModuleResourceManager* rm)
{
    const std::filesystem::path shadersDir = "Assets/";
    if (!std::filesystem::exists(shadersDir))
        return;

    auto const* material = mat.material;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersDir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".glsl") continue;

        const std::string fileName = entry.path().filename().string();
        if (fileName.rfind("BuiltIn.", 0) == 0) continue;

        const std::string fullPath = entry.path().generic_string();
        const bool isSelected =
            material->shader && material->shader->GetAssetsPath() == fullPath;

        if (ImGui::Selectable(fileName.c_str(), isSelected))
        {
            ResourceBase* resource = rm->CreateResource(fullPath);
            if (!resource)
            {
                NOUS_ERROR("InspectorWindow — failed to load shader '%s'.", fullPath.c_str());
                continue;
            }

            ApplyShader(rendererFrontend, mat, down_cast<ResourceShader*>(resource), rm);
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }
}

void ApplyShader(RendererFrontend* rendererFrontend, CMaterial& mat, ResourceShader* newEffective,
                 ModuleResourceManager* rm)
{
    if (mat.material->shader != nullptr)
        rm->UnloadResource(mat.material->shader->GetUID());

    rendererFrontend->RequestMaterialShaderChange(mat.material, newEffective);

    // Sync uniform values with the new shader's InstanceUBO:
    // keep entries whose names still exist (name-match strategy),
    // fill any missing members with defaults, and drop orphans.
    SyncUniforms(mat, newEffective);
}

void SyncUniforms(const CMaterial& mat, ResourceShader* shader)
{
    auto* material = mat.material;
    auto& uniformValues = material->uniformValues;

    const auto& setIt = shader->reflection.descriptorSets.find(1);
    if (setIt == shader->reflection.descriptorSets.end())
    {
        uniformValues.clear();
        return;
    }

    // Transparent hash + equality
    struct TransparentStringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> validNames;

    for (const auto& rb : setIt->second)
    {
        if (rb.type != DescriptorType::UniformBuffer || rb.binding != 0)
            continue;

        for (const auto& m : rb.members)
        {
            validNames.insert(m.name);

            uniformValues.try_emplace(
                m.name,
                UniformValue::MakeDefault(
                    DataTypeToUniformValueType(m.type)));
        }
        break; // Only one UBO expected
    }

    // Prune orphaned entries
    for (auto it = uniformValues.begin(); it != uniformValues.end();)
    {
        if (!validNames.contains(it->first))
            it = uniformValues.erase(it);
        else
            ++it;
    }
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

ResourceShader* GetEffectiveShader(CMaterial& mat, ModuleResourceManager* rm)
{
    ResourceShader* effectiveShader = mat.material->shader;
    if (!effectiveShader)
    {
        // Use GetLoadedResource (no ref-count bump) — the built-in material
        // shader is always loaded by the renderer, so we just borrow it.
        static uint32 s_builtInMatShaderUID = 0;
        if (s_builtInMatShaderUID == 0)
        {
            MetaFileData meta;
            if (ImportPipeline::GetAssetMetaData(
                "Assets/Shaders/BuiltIn.MaterialShader.glsl", meta))
                s_builtInMatShaderUID = meta.uid;
        }
        if (s_builtInMatShaderUID != 0)
            effectiveShader = down_cast<ResourceShader*>(
                rm->GetLoadedResource(s_builtInMatShaderUID));
    }
    return effectiveShader;
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawShaderUniforms(const CMaterial& mat, ResourceShader* effectiveShader)
{
    // ── Uniforms section ─────────────────────────────────────────
    // Mirrors the texture sampler discovery pattern: query set=1
    // binding=0 UBO members from reflection, render widget per member.
    if (!effectiveShader || effectiveShader->GetState() != ResourceState::GPU_READY)
        return;

    const ReflectedBinding* instanceUBO = nullptr;

    if (const auto it = effectiveShader->reflection.descriptorSets.find(1);
        it != effectiveShader->reflection.descriptorSets.end())
    {
        for (const auto& rb : it->second)
        {
            if (rb.type == DescriptorType::UniformBuffer && rb.binding == 0)
            {
                instanceUBO = &rb;
                break;
            }
        }
    }

    if (!instanceUBO || instanceUBO->members.empty())
        return;

    ImGui::SeparatorText("Uniforms");

    // Sort members by offset for stable display order.
    std::vector<const ReflectedMember*> sortedMembers;
    sortedMembers.reserve(instanceUBO->members.size());
    for (const auto& m : instanceUBO->members)
        sortedMembers.push_back(&m);
    std::sort(sortedMembers.begin(), sortedMembers.end(),
              [](const ReflectedMember* a, const ReflectedMember* b)
              {
                  return a->offset < b->offset;
              });

    for (const ReflectedMember* member : sortedMembers)
    {
        // Ensure the material has a value for this member.
        auto [it, inserted] = mat.material->uniformValues.try_emplace(
            member->name,
            UniformValue::MakeDefault(DataTypeToUniformValueType(member->type)));
        UniformValue& uv = it->second;

        // Heuristic: members with "color" in their name use a color picker.
        const bool isColor = (member->name.contains("color") || member->name.contains("Color"));

        ImGui::PushID(member->name.c_str());
        switch (member->type)
        {
        case DataType::Float:
            ImGui::DragFloat(member->name.c_str(), &uv.fdata.x, 0.01f);
            break;
        case DataType::Vec2:
            ImGui::DragFloat2(member->name.c_str(), &uv.fdata.x, 0.01f);
            break;
        case DataType::Vec3:
            if (isColor)
                ImGui::ColorEdit3(member->name.c_str(), &uv.fdata.x);
            else
                ImGui::DragFloat3(member->name.c_str(), &uv.fdata.x, 0.01f);
            break;
        case DataType::Vec4:
            if (isColor)
                ImGui::ColorEdit4(member->name.c_str(), &uv.fdata.x);
            else
                ImGui::DragFloat4(member->name.c_str(), &uv.fdata.x, 0.01f);
            break;
        case DataType::Int:
            ImGui::DragInt(member->name.c_str(), &uv.idata.x);
            break;
        case DataType::IVec2:
            ImGui::DragInt2(member->name.c_str(), &uv.idata.x);
            break;
        case DataType::IVec3:
            ImGui::DragInt3(member->name.c_str(), &uv.idata.x);
            break;
        case DataType::IVec4:
            ImGui::DragInt4(member->name.c_str(), &uv.idata.x);
            break;
        default:
            ImGui::TextDisabled("%s (unsupported type)", member->name.c_str());
            break;
        }
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void CMaterialDrawTextureMaps(CMaterial& mat, ModuleResourceManager* rm,
                              ResourceShader* effectiveShader)
{
    ImGui::SeparatorText("Texture Maps");

    if (!effectiveShader || effectiveShader->GetState() != ResourceState::GPU_READY)
    {
        ImGui::TextDisabled("Shader not ready.");
        return;
    }

    const auto& setIt = effectiveShader->reflection.descriptorSets.find(1);

    if (bool hasSamplers = (setIt != effectiveShader->reflection.descriptorSets.end()); !hasSamplers)
    {
        ImGui::TextDisabled("No texture slots in this shader.");
        return;
    }

    // Collect and sort samplers by binding index for stable display order.
    std::vector<const ReflectedBinding*> samplers;
    for (const auto& rb : setIt->second)
        if (rb.type == DescriptorType::CombinedImageSampler)
            samplers.push_back(&rb);
    std::sort(samplers.begin(), samplers.end(),
              [](const ReflectedBinding* a, const ReflectedBinding* b)
              {
                  return a->binding < b->binding;
              });

    for (const ReflectedBinding* rb : samplers)
    {
        ImGui::PushID(static_cast<int>(rb->binding));

        // Current texture for this slot (may be null)
        ResourceTexture const* currentTex = nullptr;
        if (auto texIt = mat.material->textureMaps.find(rb->name);
            texIt != mat.material->textureMaps.end())
            currentTex = texIt->second.texture;

        // Label + drop button
        ImGui::TextUnformatted((rb->name + ":").c_str());
        ImGui::SameLine();
        const std::string btnLabel = currentTex ? currentTex->GetName() : "(none — drop here)";
        ImGui::Button(btnLabel.c_str(), ImVec2(200.0f, 0.0f));

        CMaterialHandleTextureMapsDragDrop(mat, rm, rb, currentTex);

        // [x] clear button — only shown when a texture is assigned
        if (currentTex)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                rm->UnloadResource(currentTex->GetUID());
                mat.material->textureMaps[rb->name].texture = nullptr;
            }
        }

        ImGui::PopID();
    }
}

void CMaterialHandleTextureMapsDragDrop(
    CMaterial& mat,
    ModuleResourceManager* rm,
    const ReflectedBinding* rb,
    const ResourceTexture* currentTex)
{
    if (!ImGui::BeginDragDropTarget())
        return;

    const ImGuiPayload* payload =
        ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

    if (!payload)
    {
        ImGui::EndDragDropTarget();
        return;
    }

    std::string path = NormalizePath(payload->Data);

    if (!IsTextureFile(path))
    {
        ImGui::EndDragDropTarget();
        return;
    }

    ApplyDroppedTexture(mat, rm, rb, currentTex, path);

    ImGui::EndDragDropTarget();
}

template <typename T>
std::string NormalizePath(const T* data)
{
    std::string path(static_cast<const char*>(data));
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

bool IsTextureFile(const std::string_view path)
{
    const size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string_view::npos || dotPos + 1 >= path.size())
        return false;

    const std::string_view ext = path.substr(dotPos);

    auto iequals = [](const std::string_view a, const std::string_view b)
    {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }

        return true;
    };

    static constexpr std::array<std::string_view, 5> exts =
    {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga"
    };

    return std::ranges::any_of(exts, [&](std::string_view e)
    {
        return iequals(ext, e);
    });
}

void ApplyDroppedTexture(
    CMaterial& mat,
    ModuleResourceManager* rm,
    const ReflectedBinding* rb,
    const ResourceTexture* currentTex,
    const std::string& path)
{
    ResourceBase* r = rm->CreateResource(path);
    if (!r)
    {
        NOUS_ERROR("InspectorWindow — failed to load texture '%s'.", path.c_str());
        return;
    }

    if (currentTex)
        rm->UnloadResource(currentTex->GetUID());

    mat.material->textureMaps[rb->name].texture =
        down_cast<ResourceTexture*>(r);
}
