#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nous::engine::animation_system
{
    enum class AnimParamType : uint8_t { Float, Bool, Trigger };

    /**
     * @brief The named-value blackboard a script writes and a controller graph reads.
     *
     * Keeping it free of engine dependencies is what lets the graph's evaluator live in
     * this pure library and be tested headless.
     *
     * THE STORE ITSELF IS UNDECLARED. A controller asset declares names, types and
     * defaults, and CAnimator seeds from them, but nothing here validates a name against
     * that declaration -- so a script setting an undeclared name is SILENT: it lands in
     * the store, no condition reads it, nothing reports it. That is why the controller
     * editor offers a dropdown over declared names and the Inspector panel lists
     * declarations rather than held entries. Validating would mean this type holding a
     * pointer back to its graph, a coupling the pure layer does not need.
     */
    class AnimParameters
    {
    public:
        void  SetFloat(std::string_view name, float value);
        [[nodiscard]] float GetFloat(std::string_view name, float fallback = 0.0f) const;

        void  SetBool(std::string_view name, bool value);
        [[nodiscard]] bool  GetBool(std::string_view name, bool fallback = false) const;

        // A trigger is a bool a transition CONSUMES when it fires; it stays set until
        // consumed or reset (Unity's semantics). EvaluateController consumes the triggers
        // of the transition it fires and only that one, so at most one per frame.
        void SetTrigger(std::string_view name);
        void ResetTrigger(std::string_view name);
        [[nodiscard]] bool IsTriggerSet(std::string_view name) const;
        [[nodiscard]] bool ConsumeTrigger(std::string_view name);

        // Does this hold `name` at all, whatever its type?
        //
        // Absence is not expressible through the getters: each folds a missing name into
        // the caller's fallback, and returns that fallback for a CROSS-TYPE entry too.
        // Seeding declared defaults needs exactly that distinction -- a default must fill
        // an empty slot and must never overwrite what a script set.
        [[nodiscard]] bool Contains(std::string_view name) const { return Find(name) != nullptr; }

        [[nodiscard]] size_t Count() const { return m_entries.size(); }

    private:
        // One float backs all three types (bool and trigger are 0/1), but the type tag is
        // kept so a cross-type read returns the caller's fallback rather than a
        // plausible-looking number.
        struct Entry
        {
            uint64_t      hash  = 0;
            AnimParamType type  = AnimParamType::Float;
            float         value = 0.0f;

            // Written, never read: a debugger aid, since a store of bare hashes is
            // unreadable. It is also what a hardened Find would compare -- Find matches
            // HASHES ONLY, so a collision silently returns another parameter's value.
            std::string   name;
        };

        // Linear scan over ~5 entries beats a map and costs no per-animator allocation
        // beyond the vector. Hundreds of animated characters each carry one.
        std::vector<Entry> m_entries;

        [[nodiscard]] const Entry* Find(std::string_view name) const;
        [[nodiscard]] Entry*       Find(std::string_view name);

        // Only SETTERS call this: a getter that created entries would grow the store on
        // every polling read.
        Entry& FindOrAdd(std::string_view name, AnimParamType type);
    };
}
