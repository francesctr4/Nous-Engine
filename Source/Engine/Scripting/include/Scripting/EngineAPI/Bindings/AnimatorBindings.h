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
    // Parameters -- set by scripts, read by the controller graph (MVP-F).
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
    // ARBITRATION: the controller graph evaluates every frame; this call WINS for that
    // frame, and the graph resumes on the next one from the state this entered. There
    // is never a frame with two writers. Prefer parameters: a graph edge says when a
    // transition may happen once, where a script saying it says so at one call site.
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

    // A MULTIPLIER over each clip's own authored speed (1 = as authored, 0.5 = half,
    // negative plays backwards), not an absolute rate -- so slow motion works without
    // knowing what any clip was authored at. The authored value lives in the .nanim
    // and is edited in the Inspector; this scales it for THIS animator only.
    //
    // Per CHARACTER, not per clip: two characters sharing one clip retime
    // independently. The scene's authored value is the starting point; a script
    // setting it overrides that for the session.
    void  (*SetSpeed)(uint32_t goId, float speed) = nullptr;
    float (*GetSpeed)(uint32_t goId) = nullptr;
};

class IScriptSceneHost;
void SetupAnimatorBindings(AnimatorAPI& animator, IScriptSceneHost* sceneHost);
