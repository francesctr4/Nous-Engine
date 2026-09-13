#pragma once

#include <AnimationSystem/Controller.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Pure canvas-graph -> ControllerGraph, and the validation rules.
 *
 * Operates on plain node/link proxies naming NO imgui-node-editor types, so the same
 * code the editor runs is the code the unit tests drive. Mirrors AudioGraphLinearize,
 * and for the same reason: a flattening pass that only ever runs behind a live ImGui
 * context is a pass nobody can test, and this one decides which transition wins.
 */
namespace nous::anim_editor
{
    enum class BuildNodeKind { AnyState, State };

    struct BuildNode
    {
        uint64_t      id   = 0;
        BuildNodeKind kind = BuildNodeKind::State;

        // Everything a State node carries. Default-constructed for the AnyState node,
        // which has no state of its own -- it is a source of transitions, not a place
        // the animator can be.
        nous::engine::animation_system::ControllerState state;
    };

    struct BuildLink
    {
        uint64_t fromNodeId = 0;
        uint64_t toNodeId   = 0;

        // fromState / toState are OVERWRITTEN by BuildGraph from the node ids above;
        // everything else on it -- conditions, exit time, duration -- is authored.
        nous::engine::animation_system::ControllerTransition transition;
    };

    enum class WarningKind
    {
        StateHasNoClip,
        NoDefaultState,
        UndeclaredParameter,
        UnconditionalTransition,
        UnreachableState,
    };

    struct ValidationWarning
    {
        WarningKind kind;
        std::string subject;   // the state or parameter name the warning is about
    };

    /**
     * @brief Flattens canvas nodes and links into a graph.
     *
     * LINK ORDER IS PRESERVED, because authored order is the transition priority
     * (design §3) -- the evaluator walks transitions in order and the first satisfied
     * one wins. Never sort or group them, however tidy it would look.
     *
     * The AnyState node is not a state: it is skipped when building the state array,
     * and links out of it become `c_anyState` transitions. Including it would shift
     * every state index after it.
     *
     * A link with an endpoint the node list does not contain is DROPPED. The canvas
     * can hold one for a frame between a node being deleted and its links being
     * cleaned up, and emitting it with a stale index would index the state array.
     */
    inline nous::engine::animation_system::ControllerGraph
    BuildGraph(const std::vector<BuildNode>& nodes, const std::vector<BuildLink>& links)
    {
        namespace as = nous::engine::animation_system;

        as::ControllerGraph g;

        // node id -> state index, built FIRST so links can resolve both ends however
        // they are ordered relative to the nodes.
        std::unordered_map<uint64_t, int> stateIndex;

        bool     haveAnyState   = false;
        uint64_t anyStateNodeId = 0;

        for (const BuildNode& n : nodes)
        {
            if (n.kind == BuildNodeKind::AnyState)
            {
                haveAnyState   = true;
                anyStateNodeId = n.id;
                continue;
            }

            stateIndex[n.id] = static_cast<int>(g.states.size());
            g.states.push_back(n.state);
        }

        for (const BuildLink& l : links)
        {
            as::ControllerTransition t = l.transition;

            // A separate flag rather than testing the id against 0: an id of 0 is not
            // a value the canvas produces today, but leaning on that would make every
            // link from node 0 an Any State transition the day it does.
            if (haveAnyState && l.fromNodeId == anyStateNodeId)
            {
                t.fromState = as::ControllerGraph::c_anyState;
            }
            else
            {
                const auto from = stateIndex.find(l.fromNodeId);
                if (from == stateIndex.end())
                    continue;   // a link out of a node that no longer exists

                t.fromState = from->second;
            }

            const auto to = stateIndex.find(l.toNodeId);
            if (to == stateIndex.end())
                continue;

            t.toState = to->second;

            g.transitions.push_back(t);
        }

        return g;
    }

    /**
     * @brief Every authoring mistake the graph can express, in one pass.
     *
     * Returns empty for a well-formed graph. Order is per-state, then global, then
     * per-transition, then reachability -- stable, so the warnings panel does not
     * reshuffle under the user while they edit.
     *
     * The bar for adding a rule here: it must not fire on correct authoring. A panel
     * that cries wolf is one the user stops reading, which costs more than it buys.
     */
    inline std::vector<ValidationWarning>
    Validate(const nous::engine::animation_system::ControllerGraph& graph)
    {
        namespace as = nous::engine::animation_system;

        std::vector<ValidationWarning> out;

        // A state with no clip plays nothing and the character stands in bind pose,
        // with no other symptom anywhere.
        for (const as::ControllerState& s : graph.states)
            if (s.clipIndex < 0)
                out.push_back({ WarningKind::StateHasNoClip, s.name });

        if (!graph.IsValidState(graph.defaultState))
            out.push_back({ WarningKind::NoDefaultState, {} });

        std::unordered_set<std::string> declared;
        for (const as::ParameterDecl& p : graph.parameters)
            declared.insert(p.name);

        for (const as::ControllerTransition& t : graph.transitions)
        {
            // An undeclared name reads through AnimParameters' fallback, so Greater,
            // Less, IsTrue and TriggerSet are simply never satisfied -- but IsFalse
            // IS. So a mistyped name can produce a transition that ALWAYS fires, and
            // nothing at runtime says why.
            for (const as::ControllerCondition& c : t.conditions)
                if (!declared.contains(c.parameter))
                    out.push_back({ WarningKind::UndeclaredParameter, c.parameter });

            // Fires the frame its source state is entered, every time, so the source
            // state is never actually seen. An exit time counts as a gate here: the
            // transition waits, so the state is visible.
            if (t.conditions.empty() && !t.hasExitTime)
            {
                const std::string from =
                    t.fromState == as::ControllerGraph::c_anyState
                        ? std::string("AnyState")
                        : (graph.IsValidState(t.fromState) ? graph.states[t.fromState].name
                                                           : std::string("?"));

                out.push_back({ WarningKind::UnconditionalTransition, from });
            }
        }

        // Reachability from the default state, following Any State targets too -- an
        // attack reachable only from Any State is correctly authored and must not be
        // called an orphan.
        //
        // GATED on a valid default: without one, every state is trivially unreachable
        // and the panel would report the same single mistake N+1 times.
        if (graph.IsValidState(graph.defaultState))
        {
            std::unordered_set<int> reachable;
            std::vector<int>        frontier{ graph.defaultState };
            reachable.insert(graph.defaultState);

            for (const as::ControllerTransition& t : graph.transitions)
                if (t.fromState == as::ControllerGraph::c_anyState
                    && graph.IsValidState(t.toState)
                    && reachable.insert(t.toState).second)
                    frontier.push_back(t.toState);

            while (!frontier.empty())
            {
                const int current = frontier.back();
                frontier.pop_back();

                for (const as::ControllerTransition& t : graph.transitions)
                    if (t.fromState == current
                        && graph.IsValidState(t.toState)
                        && reachable.insert(t.toState).second)
                        frontier.push_back(t.toState);
            }

            for (size_t i = 0; i < graph.states.size(); ++i)
                if (!reachable.contains(static_cast<int>(i)))
                    out.push_back({ WarningKind::UnreachableState, graph.states[i].name });
        }

        return out;
    }
}
