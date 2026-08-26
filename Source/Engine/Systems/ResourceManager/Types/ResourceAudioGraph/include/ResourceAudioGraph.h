#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"
#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

#include <glm/glm.hpp>
#include <vector>

// CPU-only data resource: the ordered effect "recipe" authored in the
// AudioGraphEditor. No GPU residency — the per-source DSP chain is built at
// runtime by the audio backend from `effects` (Layer 2/3), not at resource load.
//
// editorPositions is opaque editor view-state (node layout), interpreted by the
// AudioGraphEditor in Layer 3 and ignored by the runtime. `generation` is bumped
// on save so playing sources can detect a change and rebuild their chain (Layer 3).
class ResourceAudioGraph : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAudioGraph(uint32 uid = 0);
    NOUS_ENGINE_API ~ResourceAudioGraph() override;

    AudioGraphDesc         effects;          // chain order == vector order
    std::vector<glm::vec2> editorPositions;  // editor view-state (Layer 3)
    uint32                 generation = 0;    // bumped on save → live rebuild (Layer 3)
};
