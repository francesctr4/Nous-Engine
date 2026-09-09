#pragma once

#include <cstdint>

// Script-facing animator control.
//
// The primary surface is PARAMETERS, not direct playback: a controller graph
// consumes named values, and a player controller is written against those. Play is
// the explicit override. Every call takes the GameObject's id, so a script can drive
// any animator in the scene, not only its owner's.
struct AnimatorAPI
{
    // Parameters -- set by scripts, read by the controller graph (MVP-F).
    void  (*SetFloat)  (uint32_t goId, const char* name, float value) = nullptr;
    float (*GetFloat)  (uint32_t goId, const char* name) = nullptr;   // 0 when unset
    void  (*SetBool)   (uint32_t goId, const char* name, bool value) = nullptr;
    bool  (*GetBool)   (uint32_t goId, const char* name) = nullptr;   // false when unset

    // A trigger stays set until a transition consumes it or ResetTrigger clears it.
    // Nothing consumes triggers until the controller graph exists.
    void  (*SetTrigger)  (uint32_t goId, const char* name) = nullptr;
    void  (*ResetTrigger)(uint32_t goId, const char* name) = nullptr;

    // Cross-fades to the named clip over fadeSeconds; <= 0 snaps. False when no clip
    // in the animator's list has that RESOURCE name.
    bool  (*Play)(uint32_t goId, const char* name, float fadeSeconds) = nullptr;

    // Query
    bool  (*IsFading)(uint32_t goId) = nullptr;

    // Writes the current clip's resource name into the caller's buffer, truncated to
    // fit and always null-terminated. Empty when nothing is playing.
    void  (*GetCurrentClip)(uint32_t goId, char* buffer, int bufferSize) = nullptr;

    // 0..1 through the current clip. During a fade this follows the OUTGOING clip,
    // matching GetCurrentClip.
    float (*GetNormalizedTime)(uint32_t goId) = nullptr;

    void  (*SetSpeed)(uint32_t goId, float speed) = nullptr;
    float (*GetSpeed)(uint32_t goId) = nullptr;
};

class IScriptSceneHost;
void SetupAnimatorBindings(AnimatorAPI& animator, IScriptSceneHost* sceneHost);
