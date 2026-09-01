#include <AnimationSystem/AnimInstance.h>

#include <AnimationSystem/AnimClip.h>
#include <AnimationSystem/Binding.h>

namespace nous::engine::animation_system
{
    void AnimInstance::SetClip(const AnimClipData* newClip, uint32_t newClipUID,
                               const AnimationBinding* newBinding)
    {
        clip    = newClip;
        clipUID = newClipUID;
        binding = newBinding;
        time    = 0.0f;

        ResetCursor();
    }

    void AnimInstance::ResetCursor()
    {
        cursor.assign(clip ? clip->channels.size() : 0, glm::uvec3(0));
    }

    void AnimInstance::Seek(float seconds)
    {
        time = seconds;

        // A seek is a discontinuity: the cursor's whole premise is that time only
        // creeps forward. FindKey self-heals on a backwards jump, but a forward jump
        // past many keys would still be walked one at a time, so reset instead.
        ResetCursor();
    }
}
