#include "Windows/InspectorWindow/InspectorComponent/ComponentInspectorRegistry/ComponentInspectorRegistry.h"
#include <EngineCore/Casts.h>

#include <EditorCore/EditorContext.h>
#include "Windows/InspectorWindow/InspectorComponent/InspectorCMaterial/InspectorCMaterial.h"
#include "Windows/InspectorWindow/InspectorComponent/InspectorCScript/InspectorCScript.h"

#include <ModuleResourceManager/ModuleResourceManager.h>
#include <ECS/Component/Component.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <ECS/GameObject.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <ResourceManager/Types/ResourceMaterial/ImporterMaterial.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>

#include "imgui.h"
#include <glm/glm.hpp>
#include <array>
#include <filesystem>
#include <string>
#include <utility>

#include <ECS/Component/Types/CAudioSource/CAudioSource.h>
#include <ECS/Component/Types/CAudioListener/CAudioListener.h>
#include <ECS/Component/Types/CVideoPlayer/CVideoPlayer.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/Component/Types/CBoneAttachment/CBoneAttachment.h>
#include <ECS/Component/Types/CPrefab/CPrefab.h>
#include <ECS/Component/Types/CPrefabLink/CPrefabLink.h>
#include <PrefabManager/PrefabManager.h>
#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <VideoSystem/AudioExtract/AudioExtract.h>

// ─────────────────────────────────────────────────────────────────────────────
// Drawers — one per inspectable component type. Uniform signature so they can be
// stored in the registry table. Each casts the Component* to its concrete type
// and pulls only the dependencies it needs from InspectorCtx.
// ─────────────────────────────────────────────────────────────────────────────

static void DrawTransform(const InspectorCtx&, Component* c)
{
    auto* ctransform = static_cast<CTransform*>(c);

    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    if (ImGui::DragFloat3("Position", &ctransform->position.x, 0.1f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    if (ImGui::DragFloat3("Rotation (Euler)", &ctransform->eulerHint.x, 0.1f))
    {
        ctransform->SetEulerRotation(ctransform->eulerHint);
        ctransform->UpdateMatrix();
    }

    if (ImGui::DragFloat3("Scale", &ctransform->scale.x, 0.01f, 0.001f, 100.0f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    ImGui::Unindent();
}

static void DrawMesh(const InspectorCtx&, Component* c)
{
    auto* cmesh = static_cast<CMesh*>(c);

    if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (!cmesh->mesh)
    {
        ImGui::TextDisabled("No mesh assigned.");
        return;
    }

    ImGui::Text("Name: %s", cmesh->mesh->GetName().c_str());
    ImGui::Text("UID: %u", cmesh->mesh->GetUID());
    ImGui::Text("Assets Path: %s", cmesh->mesh->GetAssetsPath().c_str());
    ImGui::Text("Library Path: %s", cmesh->mesh->GetLibraryPath().c_str());

    ImGui::Text("Vertices: %zu", cmesh->mesh->vertices.size());
    ImGui::Text("Indices: %zu", cmesh->mesh->indices.size());
}

static void DrawCamera(const InspectorCtx&, Component* c)
{
    auto* ccamera = static_cast<CCamera*>(c);

    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    ImGui::Checkbox("Main Camera", &ccamera->isMainCamera);

    if (ImGui::DragFloat("FOV", &ccamera->fov, 0.5f, 5.0f, 170.0f, "%.1f deg"))
        ccamera->fov = glm::clamp(ccamera->fov, 5.0f, 170.0f);

    if (ImGui::DragFloat("Near Plane", &ccamera->nearPlane, 0.01f, 0.01f, ccamera->farPlane - 0.01f, "%.3f"))
        ccamera->nearPlane = glm::max(ccamera->nearPlane, 0.001f);

    if (ImGui::DragFloat("Far Plane", &ccamera->farPlane, 1.0f, ccamera->nearPlane + 0.01f, 100000.0f, "%.1f"))
        ccamera->farPlane = glm::max(ccamera->farPlane, ccamera->nearPlane + 0.01f);

    ImGui::Unindent();
}

static void DrawLight(const InspectorCtx&, Component* c)
{
    auto* clight = static_cast<CLight*>(c);

    if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    static constexpr std::array<const char*, 3> lightTypeNames = {
        "Directional",
        "Point",
        "Spot"
    };

    if (int currentType = std::to_underlying(clight->type);
        ImGui::Combo("Type", &currentType, lightTypeNames.data(), lightTypeNames.size()))
    {
        clight->type = static_cast<LightType>(currentType);
    }

    ImGui::ColorEdit3("Color", &clight->color.r);

    ImGui::DragFloat("Intensity", &clight->intensity, 0.01f, 0.0f, 100.0f, "%.2f");
    clight->intensity = glm::max(clight->intensity, 0.0f);

    if (clight->type == LightType::Point)
    {
        ImGui::DragFloat("Range", &clight->range, 0.1f, 0.1f, 10000.0f, "%.1f");
        clight->range = glm::max(clight->range, 0.1f);
    }

    if (clight->type == LightType::Spot)
    {
        ImGui::DragFloat("Range", &clight->range, 0.1f, 0.1f, 10000.0f, "%.1f");
        clight->range = glm::max(clight->range, 0.1f);

        ImGui::DragFloat("Inner Angle", &clight->innerAngle, 0.5f, 1.0f, 89.0f, "%.1f deg");
        clight->innerAngle = glm::clamp(clight->innerAngle, 1.0f, 89.0f);

        ImGui::DragFloat("Outer Angle", &clight->outerAngle, 0.5f, 1.0f, 90.0f, "%.1f deg");
        clight->outerAngle = glm::clamp(clight->outerAngle, clight->innerAngle + 0.5f, 90.0f);
    }

    ImGui::Unindent();
}

static void DrawMaterial(const InspectorCtx& ctx, Component* c)
{
    auto* cmaterial = static_cast<CMaterial*>(c);
    const GameObject*      go               = ctx.go;
    ModuleResourceManager* rm               = ctx.rm;
    RendererFrontend*      rendererFrontend = ctx.renderer;

    if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (!cmaterial->material)
        return;

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::Button(cmaterial->material->GetName().c_str(), ImVec2(200.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const auto* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                if (std::filesystem::path(path).extension() == ".nmat")
                {
                    ResourceBase* r = rm->CreateResource(path);
                    if (r)
                    {
                        if (cmaterial->material && cmaterial->material != rm->GetDefaultMaterial())
                            rm->UnloadResource(cmaterial->material->GetUID());
                        cmaterial->material = down_cast<ResourceMaterial*>(r);
                    }
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::Text("UID: %u", cmaterial->material->GetUID());
    ImGui::Text("Assets Path: %s", cmaterial->material->GetAssetsPath().c_str());
    ImGui::Text("Library Path: %s", cmaterial->material->GetLibraryPath().c_str());

    if (cmaterial->material == rm->GetDefaultMaterial())
    {
        CMaterialDrawDefaultMaterial(go, *cmaterial, rm, ctx.editor->GetAssetsBrowserDirectory());
        return;
    }

    CMaterialDrawShaderSelector(rendererFrontend, *cmaterial, rm);

    // Determine effective shader: custom if assigned, otherwise built-in.
    ResourceShader* effectiveShader = GetEffectiveShader(*cmaterial, rm);

    CMaterialDrawShaderUniforms(*cmaterial, effectiveShader);

    CMaterialDrawTextureMaps(*cmaterial, rm, effectiveShader);

    if (ImGui::Button("Save Material"))
    {
        ImporterMaterial::SaveMaterialToAssets(cmaterial->material);
    }
}

static void DrawScript(const InspectorCtx& ctx, Component* c)
{
    auto* cscript = static_cast<CScript*>(c);

    if (!ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    DrawAttachedScripts(ctx.scene, cscript);

    ImGui::Spacing();

    DrawAddScriptSection(ctx.scriptManager, cscript);

    ImGui::Unindent();
}

static void DrawAudioSource(const InspectorCtx& ctx, Component* c)
{
    auto* cAudioSource = static_cast<CAudioSource*>(c);
    ModuleResourceManager* rm = ctx.rm;

    if (!ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    // Clip slot — assignable by dragging a .wav/.ogg from the Assets Browser.
    ImGui::Text("Clip:");
    ImGui::SameLine();
    // The "##" suffix keeps the ImGui ID unique. ImGui derives a widget's ID from its
    // LABEL, and an empty slot renders the literal text "None" -- so two empty slots in
    // one component collide ("2 visible items with conflicting ID"), and so would two
    // slots holding resources that happen to share a name. InspectorWindow's per-component
    // PushID(c) scopes components against each other, not slots within one component.
    std::string clipLabel = cAudioSource->clip ? cAudioSource->clip->GetName() : std::string("None");
    clipLabel += "##audioClipSlot";
    ImGui::Button(clipLabel.c_str(), ImVec2(200.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const auto* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                const std::string ext = std::filesystem::path(path).extension().string();
                if (ext == ".wav" || ext == ".ogg")
                {
                    if (ResourceBase* r = rm->CreateResource(path))
                    {
                        if (cAudioSource->clip)
                            rm->UnloadResource(cAudioSource->clip->GetUID());
                        cAudioSource->clip = down_cast<ResourceAudio*>(r);
                    }
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (cAudioSource->clip)
    {
        ImGui::Text("UID: %u", cAudioSource->clip->GetUID());
        ImGui::Text("Duration: %.2f s", cAudioSource->clip->GetDurationSec());
    }
    else
    {
        ImGui::TextDisabled("No clip assigned.");
    }

    // Effect graph slot — assignable by dragging a .nafx from the Assets Browser.
    ImGui::Spacing();
    ImGui::Text("Effect Graph:");
    ImGui::SameLine();
    std::string graphLabel = cAudioSource->effectGraph ? cAudioSource->effectGraph->GetName() : std::string("None");
    graphLabel += "##audioEffectGraphSlot";
    ImGui::Button(graphLabel.c_str(), ImVec2(200.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const auto* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                if (std::filesystem::path(path).extension().string() == ".nafx")
                {
                    if (ResourceBase* r = rm->CreateResource(path))
                    {
                        if (cAudioSource->effectGraph)
                            rm->UnloadResource(cAudioSource->effectGraph->GetUID());
                        cAudioSource->effectGraph = down_cast<ResourceAudioGraph*>(r);
                    }
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (cAudioSource->effectGraph)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear##fx"))
        {
            rm->UnloadResource(cAudioSource->effectGraph->GetUID());
            cAudioSource->effectGraph = nullptr;
        }
        // Re-check: Clear may have just nulled it this frame.
        if (cAudioSource->effectGraph)
            ImGui::TextDisabled("%zu effect(s)", cAudioSource->effectGraph->effects.size());
    }

    ImGui::DragFloat("Volume", &cAudioSource->volume, 0.01f, 0.0f, 2.0f, "%.2f");
    cAudioSource->volume = glm::max(cAudioSource->volume, 0.0f);

    ImGui::DragFloat("Pitch", &cAudioSource->pitch, 0.01f, 0.1f, 4.0f, "%.2f");
    cAudioSource->pitch = glm::clamp(cAudioSource->pitch, 0.1f, 4.0f);

    ImGui::Checkbox("Loop", &cAudioSource->loop);
    ImGui::Checkbox("Play On Awake", &cAudioSource->playOnAwake);

    // 3D spatialization. The distance/attenuation controls only matter when on,
    // so they're hidden behind the checkbox to keep the 2D case clean.
    ImGui::Spacing();
    ImGui::Checkbox("Spatialize (3D)", &cAudioSource->spatialize);
    if (cAudioSource->spatialize)
    {
        ImGui::DragFloat("Min Distance", &cAudioSource->minDistance, 0.1f, 0.0f, 10000.0f, "%.2f");
        cAudioSource->minDistance = glm::max(cAudioSource->minDistance, 0.0f);

        ImGui::DragFloat("Max Distance", &cAudioSource->maxDistance, 0.5f, 0.0f, 100000.0f, "%.2f");
        cAudioSource->maxDistance = glm::max(cAudioSource->maxDistance, cAudioSource->minDistance);

        static constexpr std::array<const char*, 4> attenuationNames = {
            "None", "Inverse", "Linear", "Exponential"
        };
        if (int current = static_cast<int>(cAudioSource->attenuation);
            ImGui::Combo("Attenuation", &current, attenuationNames.data(), attenuationNames.size()))
        {
            cAudioSource->attenuation = static_cast<AttenuationModel>(current);
        }
    }

    // Bus routing (Step 6). Order MUST match the AudioBus enum
    // (Master, Music, SFX, UI, Ambient).
    ImGui::Spacing();
    static constexpr std::array<const char*, 5> busNames = {
        "Master", "Music", "SFX", "UI", "Ambient"
    };
    if (int currentBus = static_cast<int>(cAudioSource->targetBus);
        ImGui::Combo("Output Bus", &currentBus, busNames.data(), busNames.size()))
    {
        cAudioSource->targetBus = static_cast<AudioBus>(currentBus);
    }

    // Edit-mode preview — audition the clip without entering play mode.
    ImGui::Spacing();
    const bool hasClip = cAudioSource->clip != nullptr;
    if (!hasClip)
        ImGui::BeginDisabled();

    if (cAudioSource->IsPreviewPlaying())
    {
        if (ImGui::Button("Stop Preview", ImVec2(120.0f, 0.0f)))
            cAudioSource->PreviewStop();
    }
    else
    {
        if (ImGui::Button("Preview", ImVec2(120.0f, 0.0f)))
            cAudioSource->PreviewPlay();
    }

    if (!hasClip)
        ImGui::EndDisabled();

    ImGui::Unindent();
}

static void DrawAudioListener(const InspectorCtx&, Component* c)
{
    auto* cListener = static_cast<CAudioListener*>(c);

    if (!ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();
    ImGui::Checkbox("Main Listener", &cListener->isMainListener);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Only one Audio Listener should be the main listener\n"
                          "(typically on the main camera).");
    ImGui::Unindent();
}

// ─────────────────────────────────────────────────────────────────────────────
// THE editor edit-site: register a component type's inspector UI here.
// `typeName` must equal Component::GetType() (guarded by t_ComponentInspectorRegistry).
// ─────────────────────────────────────────────────────────────────────────────

static void DrawVideoPlayer(const InspectorCtx& ctx, Component* c)
{
    auto* cVideo = static_cast<CVideoPlayer*>(c);
    ModuleResourceManager* rm = ctx.rm;
    GameObject* go = ctx.go;

    if (!ImGui::CollapsingHeader("Video Player", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    // Clip slot — assignable by dragging a .mp4/.gif from the Assets Browser.
    ImGui::Text("Clip:");
    ImGui::SameLine();
    ImGui::Button(cVideo->clip ? cVideo->clip->GetName().c_str() : "None", ImVec2(200.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const auto* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                const std::string ext = std::filesystem::path(path).extension().string();
                if (ext == ".mp4" || ext == ".gif")
                {
                    if (ResourceBase* r = rm->CreateResource(path))
                    {
                        if (cVideo->clip)
                            rm->UnloadResource(cVideo->clip->GetUID());
                        cVideo->clip = down_cast<ResourceVideo*>(r);

                        // Auto-pair audio: if the video carries an audio track, add a
                        // sibling CAudioSource wired to the extracted companion .ogg so
                        // the clip plays with sound out of the box. Skipped if the object
                        // already has a CAudioSource (don't clobber an authored one).
                        if (go && go->IsValid() && cVideo->clip->GetHasAudioTrack()
                            && !go->HasComponent<CAudioSource>())
                        {
                            const std::string oggPath = MakeCompanionOggPath(path);
                            if (ResourceBase* ar = rm->CreateResource(oggPath))
                            {
                                CAudioSource& audio = go->AddComponent<CAudioSource>();
                                audio.clip        = down_cast<ResourceAudio*>(ar);
                                audio.loop        = cVideo->loop;
                                audio.playOnAwake = true;
                            }
                        }
                    }
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (cVideo->clip)
    {
        ImGui::Text("UID: %u", cVideo->clip->GetUID());
        ImGui::Text("%ux%u  %.2fs  %.2f fps", cVideo->clip->GetWidth(), cVideo->clip->GetHeight(),
                    cVideo->clip->GetDurationSec(), cVideo->clip->GetFrameRate());
    }
    else
    {
        ImGui::TextDisabled("No clip assigned.");
    }

    if (ImGui::Checkbox("Loop", &cVideo->loop))
    {
        // Keep an auto-paired Audio Source's loop in lockstep — the video drives the
        // audio, so toggling the video's loop also updates a sibling CAudioSource.
        // (Pairing only matched them once; without this a later toggle would desync,
        // leaving the video looping silently after the first pass.)
        if (go && go->IsValid())
            if (CAudioSource* a = go->TryGetComponent<CAudioSource>())
                a->loop = cVideo->loop;
    }
    ImGui::Checkbox("Play On Awake", &cVideo->playOnAwake);

    // Only meaningful when the clip has an audio track (GIFs never sync).
    const bool clipHasAudio = cVideo->clip && cVideo->clip->GetHasAudioTrack();
    if (!clipHasAudio)
        ImGui::BeginDisabled();
    ImGui::Checkbox("Sync To Audio", &cVideo->syncToAudio);
    if (ImGui::IsItemHovered() && clipHasAudio)
        ImGui::SetTooltip("Slave the video playhead to a sibling Audio Source for A/V sync.\n"
                          "Uncheck to play video and audio independently.");
    if (!clipHasAudio)
        ImGui::EndDisabled();

    ImGui::Checkbox("Predecode (seamless loop)", &cVideo->predecode);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Decode every frame into memory instead of streaming.\n"
                          "Gives a seamless loop (no seek stutter at the loop point),\n"
                          "but uses frames * width * height * 4 bytes of RAM.\n"
                          "Use only for short, low-resolution clips. Takes effect on next Play.");

    // Target texture slot — choose which of the shader's texture samplers the video drives
    // (default "diffuseSampler"). Options come from SHADER REFLECTION (the material's
    // textureMaps only holds slots that already have an assigned texture, so we can't list
    // from there) — mirrors how the Material panel enumerates its texture maps.
    CMaterial* matC = (go && go->IsValid()) ? go->TryGetComponent<CMaterial>() : nullptr;
    ResourceShader* effectiveShader = (matC && matC->material) ? GetEffectiveShader(*matC, rm) : nullptr;

    if (ImGui::BeginCombo("Target Slot", cVideo->targetSlot.c_str()))
    {
        bool anySlots = false;
        if (effectiveShader)
        {
            if (const auto setIt = effectiveShader->reflection.descriptorSets.find(1);
                setIt != effectiveShader->reflection.descriptorSets.end())
            {
                for (const auto& rb : setIt->second)
                {
                    if (rb.type != DescriptorType::CombinedImageSampler) continue;
                    anySlots = true;
                    const bool selected = (rb.name == cVideo->targetSlot);
                    if (ImGui::Selectable(rb.name.c_str(), selected))
                        cVideo->targetSlot = rb.name;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
            }
        }
        if (!anySlots)
            ImGui::TextDisabled("No texture slots (add a CMaterial with a sampler shader)");
        ImGui::EndCombo();
    }
    if (!matC || !matC->material)
        ImGui::TextDisabled("No CMaterial on this object — video will not display.");

    ImGui::Unindent();
}

static void DrawAnimator(const InspectorCtx& ctx, Component* c)
{
    auto* cAnimator = static_cast<CAnimator*>(c);
    ModuleResourceManager* rm = ctx.rm;

    if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    // Returns the dropped asset's path when one matches `ext`, else empty. Same
    // ASSETS_BROWSER_ITEMS walk the audio slots use, factored out because this
    // component has two slots taking two different extensions.
    const auto acceptDrop = [](const char* ext) -> std::string
    {
        std::string result;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
            {
                const auto* data = static_cast<const char*>(payload->Data);
                const char* end = data + payload->DataSize;
                while (data < end)
                {
                    std::string path(data);
                    data += path.size() + 1;
                    if (std::filesystem::path(path).extension().string() == ext)
                    {
                        result = std::move(path);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        return result;
    };

    // Skeleton slot — assignable by dragging a .nskel from the Assets Browser.
    ImGui::Text("Skeleton:");
    ImGui::SameLine();
    // The "##" suffix keeps the ImGui ID unique. ImGui derives a widget's ID from its
    // LABEL, and an empty slot renders the literal text "None" -- so two empty slots in
    // one component collide ("2 visible items with conflicting ID"), and so would two
    // slots holding resources that happen to share a name. InspectorWindow's per-component
    // PushID(c) scopes components against each other, not slots within one component.
    std::string skeletonLabel = cAnimator->skeleton ? cAnimator->skeleton->GetName() : std::string("None");
    skeletonLabel += "##animSkeletonSlot";
    ImGui::Button(skeletonLabel.c_str(), ImVec2(200.0f, 0.0f));
    if (const std::string dropped = acceptDrop(".nskel"); !dropped.empty())
    {
        if (ResourceBase* r = rm->CreateResource(dropped))
        {
            if (cAnimator->skeleton)
                rm->UnloadResource(cAnimator->skeleton->GetUID());
            cAnimator->skeleton = down_cast<ResourceSkeleton*>(r);
        }
    }

    // Clip slot — assignable by dragging a .nanim from the Assets Browser.
    ImGui::Spacing();
    ImGui::Text("Clip:");
    ImGui::SameLine();
    std::string animClipLabel = cAnimator->clip ? cAnimator->clip->GetName() : std::string("None");
    animClipLabel += "##animClipSlot";
    ImGui::Button(animClipLabel.c_str(), ImVec2(200.0f, 0.0f));
    if (const std::string dropped = acceptDrop(".nanim"); !dropped.empty())
    {
        if (ResourceBase* r = rm->CreateResource(dropped))
        {
            if (cAnimator->clip)
                rm->UnloadResource(cAnimator->clip->GetUID());
            cAnimator->clip = down_cast<ResourceAnimation*>(r);
        }
    }

    ImGui::Spacing();
    ImGui::DragFloat("Speed", &cAnimator->speed, 0.01f, -4.0f, 4.0f);
    ImGui::Checkbox("Loop", &cAnimator->loop);

    // What the bind actually produced. This is the readout that says WHY nothing
    // is dancing: a clip binds to a skeleton by bone NAME, so a mismatched pair
    // shows plausible counts here and still animates nothing.
    ImGui::Spacing();
    if (cAnimator->skeleton)
        ImGui::Text("Bones: %zu", cAnimator->skeleton->skeleton.BoneCount());
    if (cAnimator->clip)
        ImGui::Text("Duration: %.2f s   Channels: %zu",
                    cAnimator->clip->clip.duration,
                    cAnimator->clip->clip.ChannelCount());
    if (!cAnimator->IsBound())
        ImGui::TextDisabled("Not bound — assign a skeleton and a clip.");

    ImGui::Unindent();
}

static void DrawBoneAttachment(const InspectorCtx& ctx, Component* c)
{
    auto* attachment = static_cast<CBoneAttachment*>(c);

    if (!ImGui::CollapsingHeader("Bone Attachment", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    // The same nearest-ancestor walk ComputeParentWorld performs. Done in GameObject
    // terms here because that is what the Inspector holds; the engine side works in
    // entities. Both stop at the FIRST animator found.
    CAnimator* animator = nullptr;
    if (ctx.go)
    {
        for (GameObject p = ctx.go->GetParent(); p.IsValid(); p = p.GetParent())
        {
            if (CAnimator* found = p.TryGetComponent<CAnimator>())
            {
                animator = found;
                break;
            }
        }
    }

    // The most likely user error by far is adding this component before parenting the
    // prop. An empty combo with no explanation is the one genuinely confusing failure,
    // so say what is missing instead.
    if (!animator)
    {
        ImGui::TextWrapped("No Animator above this object. Parent it under a GameObject "
                           "that has an Animator component.");
        ImGui::Unindent();
        return;
    }
    if (!animator->skeleton)
    {
        ImGui::TextWrapped("The Animator above this object has no skeleton assigned. "
                           "Drop a .nskel into its Skeleton slot.");
        ImGui::Unindent();
        return;
    }

    const std::vector<std::string>& boneNames = animator->skeleton->skeleton.names;

    std::vector<const char*> items;
    items.reserve(boneNames.size() + 1);
    items.push_back("(none)");
    for (const std::string& name : boneNames)
        items.push_back(name.c_str());

    int current = 0;   // 0 is "(none)", so bone i sits at i + 1
    for (std::size_t i = 0; i < boneNames.size(); ++i)
    {
        if (boneNames[i] == attachment->boneName)
        {
            current = static_cast<int>(i) + 1;
            break;
        }
    }

    if (ImGui::Combo("Bone", &current, items.data(), static_cast<int>(items.size())))
    {
        attachment->boneName = (current == 0) ? std::string{} : boneNames[current - 1];

        // A new name gets its own warning; otherwise correcting one mistake and then
        // making a second would warn about neither.
        attachment->warnedUnresolved = false;
    }

    // A bone the rig does not have is not visible in the combo, which otherwise just
    // shows "(none)" and looks like nothing is set.
    if (!attachment->boneName.empty() && current == 0)
        ImGui::TextWrapped("Bone '%s' is not in this skeleton.", attachment->boneName.c_str());

    ImGui::TextDisabled("This object's Transform is its offset, in bone space.");

    ImGui::Unindent();
}

static void DrawPrefab(const InspectorCtx& ctx, Component* c)
{
    auto* cprefab = static_cast<CPrefab*>(c);

    if (!ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    ImGui::TextDisabled("Source");
    ImGui::TextWrapped("%s", cprefab->prefabSourcePath.c_str());
    ImGui::Spacing();

    // Three states, and the third is not a failure: an instance from a scene saved
    // before prefab overrides has no links, and Update relinks it once.
    const bool linked = ctx.go && ctx.go->HasComponent<CPrefabLink>();
    if (!linked)
        ImGui::TextWrapped("Not linked — Update will rebuild this instance once to link it.");
    else if (cprefab->isStale)
        ImGui::TextWrapped("Out of date — the prefab asset has changed since this instance was synced.");
    else
        ImGui::TextDisabled("In sync.");

    ImGui::Spacing();

    if (ImGui::Button("Update from Prefab"))
        PrefabManager::UpdateFromPrefab(*ctx.go, ctx.scene);

    ImGui::SameLine();

    if (ImGui::Button("Apply to Prefab"))
        PrefabManager::ApplyToPrefab(*ctx.go, ctx.scene);

    ImGui::TextDisabled("Update overwrites objects the prefab owns; your additions are kept.");

    ImGui::Unindent();
}

// Read-only: this is engine bookkeeping, and showing it is what makes "which objects
// will Update overwrite?" answerable.
static void DrawPrefabLink(const InspectorCtx&, Component* c)
{
    auto* link = static_cast<CPrefabLink*>(c);

    if (!ImGui::CollapsingHeader("Prefab Link"))
        return;

    ImGui::Indent();
    ImGui::TextDisabled("Owned by the prefab, object id %u.", link->prefabObjectID);
    ImGui::Unindent();
}

static const ComponentUI k_ui[] = {
    {"CTransform", "Transform", false, &DrawTransform},
    {"CMesh",      "Mesh",      true,  &DrawMesh},
    {"CCamera",    "Camera",    true,  &DrawCamera},
    {"CLight",     "Light",     true,  &DrawLight},
    {"CMaterial",  "Material",  true,  &DrawMaterial},
    {"CScript",    "Script",    true,  &DrawScript},
    {"CAudioSource",    "Audio Source",    true,  &DrawAudioSource},
    {"CAudioListener",  "Audio Listener",  true,  &DrawAudioListener},
    {"CVideoPlayer",    "Video Player",    true,  &DrawVideoPlayer},
    {"CAnimator",       "Animator",        true,  &DrawAnimator},
    {"CBoneAttachment", "Bone Attachment", true,  &DrawBoneAttachment},
    // Neither is user-addable: CPrefab is attached by InstantiatePrefab, and a
    // hand-added CPrefabLink would claim prefab ownership of an object the prefab
    // has never heard of.
    {"CPrefab",         "Prefab",          false, &DrawPrefab},
    {"CPrefabLink",     "Prefab Link",     false, &DrawPrefabLink},
};

const ComponentUI* FindComponentUI(std::string_view typeName)
{
    for (const ComponentUI& e : k_ui)
        if (e.typeName == typeName)
            return &e;
    return nullptr;
}

std::span<const ComponentUI> ComponentUITable()
{
    return k_ui;
}
