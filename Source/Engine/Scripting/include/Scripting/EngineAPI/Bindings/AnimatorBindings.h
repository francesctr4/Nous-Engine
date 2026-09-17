#pragma once

#include <cstdint>

// Script-facing animator control.
//
// The primary surface is PARAMETERS, not direct playback: a controller graph
// consumes named values, and a player controller is written against those. CrossFade
// is the explicit override. Every call takes the GameObject's id, so a script can
// drive any animator in the scene, not only its owner's.
struct AnimatorAPI
{
    // Parameters -- set by scripts, read by the controller graph's transition
    // conditions. A name the controller does not declare is accepted and never read:
    // the declared list is in the .nctrl, and the editor's dropdown is where a typo
    // is meant to be caught.
    void  (*SetFloat)  (uint32_t goId, const char* name, float value) = nullptr;
    float (*GetFloat)  (uint32_t goId, const char* name) = nullptr;   // 0 when unset
    void  (*SetBool)   (uint32_t goId, const char* name, bool value) = nullptr;
    bool  (*GetBool)   (uint32_t goId, const char* name) = nullptr;   // false when unset

    // A trigger stays set until a transition consumes it or ResetTrigger clears it.
    // The controller graph consumes the trigger of the transition it fires -- at most
    // one per frame, since the first satisfied transition wins.
    void  (*SetTrigger)  (uint32_t goId, const char* name) = nullptr;
    void  (*ResetTrigger)(uint32_t goId, const char* name) = nullptr;

    // Cross-fades to the named STATE of the animator's controller over fadeSeconds;
    // <= 0 snaps. False when the animator has no controller or no state has that name.
    //
    // ARBITRATION: the graph evaluates every frame, but this call WINS for that frame and
    // the graph resumes on the next one from the state this entered. Prefer parameters --
    // a graph edge states a transition once, where a script states it per call site.
    bool  (*CrossFade)(uint32_t goId, const char* stateName, float fadeSeconds) = nullptr;

    // Query
    bool  (*IsFading)(uint32_t goId) = nullptr;

    // Writes the current STATE's name into the caller's buffer, truncated to fit and
    // always null-terminated. Empty when there is no controller, or the graph has no
    // valid current state.
    //
    // During a transition this is the DESTINATION state: it becomes current the
    // instant the transition starts, which is what makes a transition's exit time
    // measure the state being entered.
    void  (*GetCurrentState)(uint32_t goId, char* buffer, int bufferSize) = nullptr;

    // 0..1 through the current state's clip. During a fade this follows the INCOMING
    // clip, matching GetCurrentState.
    float (*GetNormalizedTime)(uint32_t goId) = nullptr;

    // A MULTIPLIER over each clip's authored speed (1 = as authored, negative plays
    // backwards), not an absolute rate -- so slow motion works without knowing what any
    // clip was authored at. Per CHARACTER, not per clip: two characters sharing one clip
    // retime independently.
    void  (*SetSpeed)(uint32_t goId, float speed) = nullptr;
    float (*GetSpeed)(uint32_t goId) = nullptr;
};

class IScriptSceneHost;
void SetupAnimatorBindings(AnimatorAPI& animator, IScriptSceneHost* sceneHost);
