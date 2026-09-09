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
     * Scripts set values ("speed", "isGrounded", "jump"); the controller graph
     * (MVP-F) tests them in transition conditions. This type is the whole reason
     * the graph's evaluator can live in this pure library and be tested headless --
     * keep it free of engine dependencies.
     *
     * Parameters are UNDECLARED for now: any name can be set. MVP-F's controller
     * asset will declare names, types and defaults, and validation arrives with it.
     */
    class AnimParameters
    {
    public:
        void  SetFloat(std::string_view name, float value);
        [[nodiscard]] float GetFloat(std::string_view name, float fallback = 0.0f) const;

        void  SetBool(std::string_view name, bool value);
        [[nodiscard]] bool  GetBool(std::string_view name, bool fallback = false) const;

        // A trigger is a bool a transition CONSUMES when it fires. It stays set until
        // consumed or reset -- Unity's semantics, chosen so anyone arriving from Unity
        // carries the right mental model. Nothing consumes triggers until MVP-F.
        void SetTrigger(std::string_view name);
        void ResetTrigger(std::string_view name);
        [[nodiscard]] bool IsTriggerSet(std::string_view name) const;
        [[nodiscard]] bool ConsumeTrigger(std::string_view name);

        [[nodiscard]] size_t Count() const { return m_entries.size(); }

    private:
        // One float backs all three types (bool and trigger are 0/1), but the type tag
        // is kept: a cross-type read returns the caller's fallback rather than a
        // plausible-looking number, so a script bug stays visible.
        struct Entry
        {
            uint64_t      hash  = 0;
            AnimParamType type  = AnimParamType::Float;
            float         value = 0.0f;

            // Kept beside the hash purely for diagnostics -- a mistyped parameter is
            // otherwise a silent hash miss, and MVP-F's editor will want to list what a
            // running animator actually holds. Once the controller asset declares
            // parameters, values can move to index-addressed storage and this can go.
            std::string   name;
        };

        // Linear scan over ~5 entries beats a map and costs no per-animator heap
        // allocation beyond the vector itself. Hundreds of animated characters each
        // carry one of these.
        std::vector<Entry> m_entries;

        [[nodiscard]] const Entry* Find(std::string_view name) const;
        [[nodiscard]] Entry*       Find(std::string_view name);

        // Returns the existing entry or appends one. Only SETTERS call this -- a
        // getter that created entries would grow the store on every polling read.
        Entry& FindOrAdd(std::string_view name, AnimParamType type);
    };
}
