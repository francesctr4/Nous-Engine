#include <ResourceManager/Types/ResourceAnimationController/ImporterAnimationController.h>
#include <EngineCore/Casts.h>

#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <ResourceManager/Runtime/ImportPipeline.h>

#include <AnimationSystem/AnimParameters.h>

#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <cstddef>
#include <string>
#include <utility>

namespace as = nous::engine::animation_system;

namespace
{
    // Any State is a SENTINEL index in memory and a reserved literal on disk. It
    // cannot be a state index in the file for the same reason nothing else is:
    // indices do not survive reordering and read as noise in a diff.
    constexpr const char* c_anyStateToken = "AnyState";

    // ---- enum <-> string ----
    //
    // Enums serialize as STRINGS throughout, following CLight and CAudioSource. A
    // value inserted mid-enum must not silently change the meaning of assets
    // already on disk -- which is exactly what an integer would do, with no error.

    const char* ComparatorToString(const as::ConditionComparator c)
    {
        switch (c)
        {
            case as::ConditionComparator::Less:       return "Less";
            case as::ConditionComparator::IsTrue:     return "IsTrue";
            case as::ConditionComparator::IsFalse:    return "IsFalse";
            case as::ConditionComparator::TriggerSet: return "TriggerSet";
            case as::ConditionComparator::Greater:    // fallthrough
            default:                                  return "Greater";
        }
    }

    as::ConditionComparator ComparatorFromString(const std::string& s)
    {
        if (s == "Less")       return as::ConditionComparator::Less;
        if (s == "IsTrue")     return as::ConditionComparator::IsTrue;
        if (s == "IsFalse")    return as::ConditionComparator::IsFalse;
        if (s == "TriggerSet") return as::ConditionComparator::TriggerSet;
        return as::ConditionComparator::Greater;
    }

    const char* ParamTypeToString(const uint8_t t)
    {
        switch (static_cast<as::AnimParamType>(t))
        {
            case as::AnimParamType::Bool:    return "Bool";
            case as::AnimParamType::Trigger: return "Trigger";
            case as::AnimParamType::Float:   // fallthrough
            default:                         return "Float";
        }
    }

    uint8_t ParamTypeFromString(const std::string& s)
    {
        if (s == "Bool")    return static_cast<uint8_t>(as::AnimParamType::Bool);
        if (s == "Trigger") return static_cast<uint8_t>(as::AnimParamType::Trigger);
        return static_cast<uint8_t>(as::AnimParamType::Float);
    }

    // ControllerState::rootMotion travels as the raw RootMotionMode enum VALUE --
    // the resource layer must not include the ECS header that declares it. 0 is
    // Inherit, which is why Inherit is the first enumerator: an unspecified state
    // reads as "use the component's mode" with no conversion table.
    const char* RootMotionToToken(const int mode)
    {
        switch (mode)
        {
            case 1:  return "Baked";
            case 2:  return "Applied";
            case 3:  return "InPlace";
            case 0:  // fallthrough
            default: return "Inherit";
        }
    }

    int RootMotionFromToken(const std::string& s)
    {
        if (s == "Baked")   return 1;
        if (s == "Applied") return 2;
        if (s == "InPlace") return 3;
        return 0;   // Inherit, and also the "key was absent" answer
    }
}

bool ImporterAnimationController::Import(const MetaFileData& metaFileData)
{
    ResourceBase* temp = NOUS_NEW<ResourceAnimationController>(MemoryTag::RESOURCE_ANIM_CONTROLLER);
    return Save(metaFileData, temp);
}

bool ImporterAnimationController::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_ANIM_CONTROLLER);

    // NOT a verbatim copy, unlike ImporterAudioGraph: a controller references peer
    // .nanim assets, so the Library copy is ENRICHED with each clip's uid and
    // library path. An exported game ships no Assets/ and resolves clips from those
    // two fields alone -- the authored assetPath is unreachable there.
    JsonObject root = JsonFile::LoadFromFile(metaFileData.assetsPath);
    if (root.IsEmpty())
    {
        NOUS_ERROR("ImporterAnimationController::Save() failed to load '%s'",
                   metaFileData.assetsPath.c_str());
        return false;
    }

    JsonArray statesArr = root.GetArray("states");
    JsonArray enriched;

    for (int i = 0; i < statesArr.Count(); ++i)
    {
        JsonObject s = statesArr.GetObject(i);

        JsonObject clip = s.GetObject("clip");
        const std::string clipAsset = clip.GetString("assetPath");

        if (!clipAsset.empty())
        {
            MetaFileData clipMeta;
            if (ImportPipeline::GetAssetMetaData(clipAsset, clipMeta))
            {
                clip.Set("uid", static_cast<double>(clipMeta.uid));
                clip.Set("libraryPath", clipMeta.libraryPath);
            }
            else
            {
                // The clip asset is missing or not imported. Leave what was authored
                // rather than blanking it: a broken reference the user can see and
                // fix beats a slot that silently forgot what it pointed at.
                NOUS_WARN("ImporterAnimationController::Save() -- clip '%s' has no .meta; "
                          "leaving its authored slot unenriched.", clipAsset.c_str());
            }
            s.Set("clip", std::move(clip));
        }

        enriched.Append(std::move(s));
    }

    root.Set("states", std::move(enriched));

    return JsonFile::SaveToFile(root, metaFileData.libraryPath);
}

bool ImporterAnimationController::Deserialize(const std::string& libraryPath, ResourceBase* resource)
{
    ResourceAnimationController* controller = down_cast<ResourceAnimationController*>(resource);
    if (!controller) return false;

    JsonObject root = JsonFile::LoadFromFile(libraryPath);
    if (root.IsEmpty())
    {
        NOUS_ERROR("ImporterAnimationController::Deserialize() failed to load '%s'", libraryPath.c_str());
        return false;
    }

    as::ControllerGraph& graph = controller->graph;

    graph.states.clear();
    graph.transitions.clear();
    graph.parameters.clear();
    graph.defaultState = -1;
    controller->clipSlots.clear();
    controller->editorPositions.clear();

    // -- parameters[] --
    JsonArray paramsArr = root.GetArray("parameters");
    for (int i = 0; i < paramsArr.Count(); ++i)
    {
        const JsonObject p = paramsArr.GetObject(i);
        if (p.IsEmpty()) continue;

        as::ParameterDecl decl;
        decl.name         = p.GetString("name");
        decl.type         = ParamTypeFromString(p.GetString("type", "Float"));
        decl.defaultValue = p.GetFloat("default", 0.0f);

        if (decl.name.empty()) continue;   // an unnamed declaration addresses nothing

        graph.parameters.push_back(std::move(decl));
    }

    // -- states[] --
    //
    // Every Get* takes a fallback: an absent key means DEFAULT, never corrupt. An
    // asset hand-edited down to a name must still load, the same rule the .nanim
    // stub follows.
    JsonArray statesArr = root.GetArray("states");
    for (int i = 0; i < statesArr.Count(); ++i)
    {
        const JsonObject s = statesArr.GetObject(i);
        if (s.IsEmpty()) continue;

        as::ControllerState state;
        state.name           = s.GetString("name");
        state.speed          = s.GetFloat("speed", 1.0f);
        state.speedParameter = s.GetString("speedParameter");
        state.rootMotion     = RootMotionFromToken(s.GetString("rootMotion", "Inherit"));

        // clipIndex is NOT serialized: it is derived when the clip resolves (Task 5).
        // A controller read with no resource manager therefore has every state
        // unplayable rather than pointing into an empty clips array.
        state.clipIndex = -1;

        const JsonObject clip = s.GetObject("clip");
        ControllerClipSlot slot;
        slot.assetPath   = clip.GetString("assetPath");
        slot.libraryPath = clip.GetString("libraryPath");
        slot.uid         = static_cast<uint32_t>(clip.GetDouble("uid", 0.0));

        const JsonObject editor = s.GetObject("editor");

        graph.states.push_back(std::move(state));
        controller->clipSlots.push_back(std::move(slot));
        controller->editorPositions.emplace_back(editor.GetFloat("x", 0.0f),
                                                 editor.GetFloat("y", 0.0f));
    }

    // -- transitions[] --
    //
    // Endpoints are STATE NAMES on disk and indices in memory, resolved here now
    // that every state exists. An unresolvable name becomes -1 rather than failing
    // the load: EvaluateController skips such a transition, and the editor reports
    // it, so one stale link does not cost the user the whole asset.
    JsonArray transitionsArr = root.GetArray("transitions");
    for (int i = 0; i < transitionsArr.Count(); ++i)
    {
        const JsonObject t = transitionsArr.GetObject(i);
        if (t.IsEmpty()) continue;

        as::ControllerTransition transition;

        const std::string fromName = t.GetString("from");
        if (fromName == c_anyStateToken)
        {
            transition.fromState = as::ControllerGraph::c_anyState;
        }
        else
        {
            transition.fromState = graph.FindState(fromName);
            if (transition.fromState < 0)
                NOUS_WARN("ImporterAnimationController: transition source '%s' names no state in '%s'.",
                          fromName.c_str(), libraryPath.c_str());
        }

        const std::string toName = t.GetString("to");
        transition.toState = graph.FindState(toName);
        if (transition.toState < 0)
            NOUS_WARN("ImporterAnimationController: transition target '%s' names no state in '%s'.",
                      toName.c_str(), libraryPath.c_str());

        transition.hasExitTime = t.GetBool("hasExitTime", false);
        transition.exitTime    = t.GetFloat("exitTime", 0.8f);
        transition.duration    = t.GetFloat("duration", 0.2f);

        JsonArray conditionsArr = t.GetArray("conditions");
        for (int c = 0; c < conditionsArr.Count(); ++c)
        {
            const JsonObject cond = conditionsArr.GetObject(c);
            if (cond.IsEmpty()) continue;

            as::ControllerCondition condition;
            condition.parameter  = cond.GetString("parameter");
            condition.comparator = ComparatorFromString(cond.GetString("comparator", "Greater"));
            condition.value      = cond.GetFloat("value", 0.0f);

            transition.conditions.push_back(std::move(condition));
        }

        graph.transitions.push_back(std::move(transition));
    }

    // -- defaultState --
    //
    // A name, resolved last. Unresolvable is -1, which CAnimator falls back out of
    // and the editor warns about; it is not a load failure.
    const std::string defaultName = root.GetString("defaultState");
    graph.defaultState = defaultName.empty() ? -1 : graph.FindState(defaultName);

    ResolveClips(controller);

    return true;
}

void ImporterAnimationController::ResolveClips(ResourceAnimationController* controller)
{
    IResourceLoader* rm = m_resources;
    if (!rm)
    {
        // No resource system (unit tests, and the editor before injection). Every
        // state is unplayable rather than pointing into an empty clips array.
        for (as::ControllerState& state : controller->graph.states)
            state.clipIndex = -1;
        return;
    }

    // THE HAZARD, on the record because this exact bug has shipped twice in this
    // tree: Deserialize is NOT called only on a fresh resource -- the asset
    // hot-reload path re-deserializes a LIVE controller in place. Every pass
    // re-acquires each clip, so every pass must give back what the slots already
    // held, UNCONDITIONALLY and AFTER the acquire.
    //
    // Holding the old vector aside and draining it at the end makes both halves of
    // that structural rather than per-slot:
    //   - unconditional, because nothing here compares old against new. The
    //     `previous != clip` guard is the obvious version and leaks in exactly the
    //     common case: re-resolving finds the SAME clip resident and only
    //     increments, so a change-detecting release never fires.
    //   - after, because the drain cannot run until every acquire has. Releasing
    //     first would let a count transiently hit 0 and queue a spurious eviction.
    // It also survives a state being added or removed between passes, which a
    // per-slot release keyed on index does not.
    std::vector<ResourceAnimation*> previous;
    previous.swap(controller->clips);

    controller->clips.assign(controller->graph.states.size(), nullptr);

    for (size_t i = 0; i < controller->graph.states.size(); ++i)
    {
        as::ControllerState&      state = controller->graph.states[i];
        const ControllerClipSlot& slot  = controller->clipSlots[i];

        ResourceAnimation* clip = nullptr;

        if (!slot.libraryPath.empty() && slot.uid != 0)
            clip = down_cast<ResourceAnimation*>(
                rm->CreateResourceFromLibrary(slot.uid, ResourceType::ANIMATION,
                                              nous::engine::filesystem::GetFilename(slot.assetPath),
                                              slot.assetPath, slot.libraryPath));
        else if (!slot.assetPath.empty())
            clip = down_cast<ResourceAnimation*>(rm->CreateResource(slot.assetPath));

        if (clip)
        {
            controller->clips[i] = clip;
            state.clipIndex      = static_cast<int>(i);

            NOUS_DEBUG("[RefTrace] +CLIP uid=%u refs=%u  (state '%s' of controller '%s' deserialized)",
                       clip->GetUID(), clip->GetReferenceCount(),
                       state.name.c_str(), controller->GetName().c_str());
        }
        else
        {
            state.clipIndex = -1;   // unplayable state, not an error

            if (!slot.assetPath.empty())
                NOUS_WARN("ImporterAnimationController: state '%s' clip '%s' could not be loaded.",
                          state.name.c_str(), slot.assetPath.c_str());
        }
    }

    for (ResourceAnimation* old : previous)
    {
        if (!old) continue;

        NOUS_DEBUG("[RefTrace] -CLIP uid=%u refs=%u  (controller '%s' re-deserialized in place)",
                   old->GetUID(), old->GetReferenceCount(), controller->GetName().c_str());
        rm->UnloadResource(old->GetUID());
    }
}

void ImporterAnimationController::Evict(ResourceBase* resource)
{
    ResourceAnimationController* controller = down_cast<ResourceAnimationController*>(resource);
    if (!controller) return;

    for (ResourceAnimation*& clip : controller->clips)
    {
        if (clip && m_resources)
        {
            NOUS_DEBUG("[RefTrace] -CLIP uid=%u refs=%u  (controller '%s' evicted)",
                       clip->GetUID(), clip->GetReferenceCount(), controller->GetName().c_str());
            m_resources->UnloadResource(clip->GetUID());
        }

        // Nulled even when there is no resource system, so a later pass cannot
        // double-release a pointer this one already gave back.
        clip = nullptr;
    }

    for (as::ControllerState& state : controller->graph.states)
        state.clipIndex = -1;
}

bool ImporterAnimationController::Upload(ResourceBase* /*resource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU residency -- a controller is a decision table. Same as ImporterAudioGraph.
    return true;
}

void ImporterAnimationController::Release(ResourceBase* /*resource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU handles held; nothing to release.
}

bool ImporterAnimationController::WriteControllerToFile(const ResourceAnimationController& controller,
                                                        const std::string& path)
{
    const as::ControllerGraph& graph = controller.graph;

    // Turns a state index into what goes in a transition endpoint. An index that
    // does not resolve writes an empty name, which reads back as -1 -- a broken
    // link the editor can show, rather than a link silently repointed at state 0.
    const auto stateName = [&graph](const int index) -> std::string
    {
        return graph.IsValidState(index) ? graph.states[index].name : std::string();
    };

    JsonObject root;

    root.Set("defaultState", stateName(graph.defaultState));

    JsonArray paramsArr;
    for (const as::ParameterDecl& decl : graph.parameters)
    {
        JsonObject p;
        p.Set("name", decl.name);
        p.Set("type", ParamTypeToString(decl.type));
        p.Set("default", decl.defaultValue);
        paramsArr.Append(std::move(p));
    }
    root.Set("parameters", std::move(paramsArr));

    JsonArray statesArr;
    for (size_t i = 0; i < graph.states.size(); ++i)
    {
        const as::ControllerState& state = graph.states[i];

        JsonObject s;
        s.Set("name", state.name);
        s.Set("speed", state.speed);
        s.Set("speedParameter", state.speedParameter);
        s.Set("rootMotion", RootMotionToToken(state.rootMotion));

        // The clip block comes from the AUTHORED slot, refreshed from the resolved
        // resource when there is one. Reading only the resolved pointer would blank
        // the binding of any state whose asset is currently missing.
        ControllerClipSlot slot;
        if (i < controller.clipSlots.size())
            slot = controller.clipSlots[i];

        if (state.clipIndex >= 0 && static_cast<size_t>(state.clipIndex) < controller.clips.size())
        {
            if (const ResourceAnimation* clip = controller.clips[state.clipIndex])
            {
                slot.assetPath   = clip->GetAssetsPath();
                slot.libraryPath = clip->GetLibraryPath();
                slot.uid         = clip->GetUID();
            }
        }

        JsonObject clipObj;
        clipObj.Set("assetPath", slot.assetPath);
        clipObj.Set("libraryPath", slot.libraryPath);
        clipObj.Set("uid", static_cast<double>(slot.uid));
        s.Set("clip", std::move(clipObj));

        JsonObject editor;
        const glm::vec2 pos = i < controller.editorPositions.size()
                                  ? controller.editorPositions[i]
                                  : glm::vec2(0.0f);
        editor.Set("x", pos.x);
        editor.Set("y", pos.y);
        s.Set("editor", std::move(editor));

        statesArr.Append(std::move(s));
    }
    root.Set("states", std::move(statesArr));

    JsonArray transitionsArr;
    for (const as::ControllerTransition& t : graph.transitions)
    {
        JsonObject entry;

        entry.Set("from", t.fromState == as::ControllerGraph::c_anyState
                              ? std::string(c_anyStateToken)
                              : stateName(t.fromState));
        entry.Set("to", stateName(t.toState));
        entry.Set("hasExitTime", t.hasExitTime);
        entry.Set("exitTime", t.exitTime);
        entry.Set("duration", t.duration);

        JsonArray conditionsArr;
        for (const as::ControllerCondition& c : t.conditions)
        {
            JsonObject cond;
            cond.Set("parameter", c.parameter);
            cond.Set("comparator", ComparatorToString(c.comparator));
            cond.Set("value", c.value);
            conditionsArr.Append(std::move(cond));
        }
        entry.Set("conditions", std::move(conditionsArr));

        transitionsArr.Append(std::move(entry));
    }
    root.Set("transitions", std::move(transitionsArr));

    return JsonFile::SaveToFile(root, path);
}

bool ImporterAnimationController::CreateNewControllerFile(const std::string& assetPath)
{
    if (assetPath.empty()) return false;

    nous::engine::filesystem::CreateDirectory(nous::engine::filesystem::GetDirectory(assetPath));

    JsonObject root;
    root.Set("defaultState", "");
    root.Set("parameters", JsonArray{});
    root.Set("states", JsonArray{});
    root.Set("transitions", JsonArray{});

    return JsonFile::SaveToFile(root, assetPath);
}
