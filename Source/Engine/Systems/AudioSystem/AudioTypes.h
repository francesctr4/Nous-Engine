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
