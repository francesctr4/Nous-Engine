#include "Editor/UI/Windows/AudioMixer/include/AudioMixerWindow.h"

#include "Editor/EditorContext.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"
#include "Engine/Systems/AudioSystem/AudioTypes.h"
#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/CAudioSource.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct Strip { AudioBus bus; const char* label; bool hasSolo; };

    // Master first (no solo), then the four named buses.
    constexpr std::array<Strip, 5> k_strips = {{
        { AudioBus::Master,  "Master",  false },
        { AudioBus::Music,   "Music",   true  },
        { AudioBus::SFX,     "SFX",     true  },
        { AudioBus::UI,      "UI",      true  },
        { AudioBus::Ambient, "Ambient", true  },
    }};

    constexpr float k_minDb = -80.0f;  // floor == silence
    constexpr float k_maxDb =  20.0f;  // +20 dB == 10x amplification

    // dB <-> linear (no miniaudio in the editor module). -80 dB maps to silence.
    float LinearToDb(float linear)
    {
        return linear <= 0.0f ? k_minDb : 20.0f * std::log10(linear);
    }
    float DbToLinear(float db)
    {
        return db <= k_minDb ? 0.0f : std::pow(10.0f, db / 20.0f);
    }

    // One channel strip. Reads live state from the backend each frame.
    void DrawStrip(const Strip& s, ModuleAudio& audio)
    {
        ImGui::PushID(s.label);
        ImGui::BeginGroup();

        ImGui::TextUnformatted(s.label);

        // Vertical dB fader. Derive dB from the live linear gain each frame.
        float db = LinearToDb(audio.GetBusVolume(s.bus));
        db = std::max(k_minDb, std::min(k_maxDb, db));
        if (ImGui::VSliderFloat("##fader", ImVec2(40.0f, 200.0f), &db, k_minDb, k_maxDb, "%.0f"))
            audio.SetBusVolume(s.bus, DbToLinear(db));

        ImGui::Text("%.1f dB", db);

        // Solo / Mute toggle buttons (colored when active).
        const bool muted  = audio.GetBusMute(s.bus);
        const bool soloed = s.hasSolo && audio.GetBusSolo(s.bus);

        if (s.hasSolo)
        {
            if (soloed) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.10f, 1.0f));
            if (ImGui::Button("S")) audio.SetBusSolo(s.bus, !soloed);
            if (soloed) ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        if (muted) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("M")) audio.SetBusMute(s.bus, !muted);
        if (muted) ImGui::PopStyleColor();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    // Left-side routing panel: which scene CAudioSources feed each bus, with a live
    // ">" indicator while a source's play-driven voice is actually playing. Pure
    // editor read of the ECS — no new runtime, no miniaudio. Mirrors the bus order of
    // k_strips (== AudioBus enum order), so byBus index == static_cast<int>(targetBus).
    void DrawRoutingPanel(ModuleScene& scene)
    {
        ImGui::BeginChild("##routing", ImVec2(170.0f, 0.0f), true);

        ImGui::TextUnformatted("Sources");
        ImGui::Separator();

        Scene* active = scene.activeScene;
        if (!active)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::EndChild();
            return;
        }

        std::array<std::vector<std::pair<std::string, bool>>, k_strips.size()> byBus;
        for (GameObject go : active->GetGameObjectsSnapshot())
        {
            CAudioSource* src = go.TryGetComponent<CAudioSource>();
            if (!src)
                continue;

            const int idx = static_cast<int>(src->targetBus);
            if (idx >= 0 && idx < static_cast<int>(byBus.size()))
                byBus[idx].emplace_back(go.GetName(), src->IsVoicePlaying());
        }

        for (size_t i = 0; i < k_strips.size(); ++i)
        {
            ImGui::Spacing();
            ImGui::Text("%s (%d)", k_strips[i].label, static_cast<int>(byBus[i].size()));

            if (byBus[i].empty())
            {
                ImGui::TextDisabled("    -");
                continue;
            }

            for (const auto& [name, playing] : byBus[i])
            {
                if (playing)
                    ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.30f, 1.0f), "  > %s", name.c_str());
                else
                    ImGui::Text("    %s", name.c_str());
            }
        }

        ImGui::EndChild();
    }
}

AudioMixerWindow::AudioMixerWindow(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void AudioMixerWindow::Update()
{
    ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_FirstUseEver);
}

void AudioMixerWindow::DrawContent()
{
    ModuleScene* scene = editorContext ? editorContext->GetScene() : nullptr;
    ModuleAudio* audio = scene ? scene->GetAudio() : nullptr;

    if (!audio)
    {
        ImGui::TextUnformatted("Audio backend unavailable.");
        return;
    }

    DrawRoutingPanel(*scene);
    ImGui::SameLine(0.0f, 12.0f);

    ImGui::BeginGroup();
    for (size_t i = 0; i < k_strips.size(); ++i)
    {
        DrawStrip(k_strips[i], *audio);
        if (i + 1 < k_strips.size())
            ImGui::SameLine(0.0f, 18.0f);
    }
    ImGui::EndGroup();
}
