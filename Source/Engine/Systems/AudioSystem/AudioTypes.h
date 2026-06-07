#pragma once

// Distance attenuation model for a spatialized audio voice. Dependency-free
// (like SoundHandle.h) so components and the backend interface can name it
// without pulling in miniaudio. MiniaudioBackend maps these to ma_attenuation_model.
enum class AttenuationModel
{
    None,         // no distance attenuation (constant gain)
    Inverse,      // 1/d falloff — most natural, the default
    Linear,       // linear falloff between min/max distance
    Exponential   // steep falloff
};

// Mixer bus a voice routes through. Dependency-free (like AttenuationModel) so
// components, the module, the backend interface, and the editor name it without
// pulling in miniaudio. Master is the engine endpoint itself (no ma_sound_group);
// the other four are groups parented to it. Enum value is used as the state index
// in MiniaudioBusGraph (Master=0 .. Ambient=4) — do not reorder without updating it.
enum class AudioBus
{
    Master,
    Music,
    SFX,      // default for CAudioSource
    UI,
    Ambient
};
