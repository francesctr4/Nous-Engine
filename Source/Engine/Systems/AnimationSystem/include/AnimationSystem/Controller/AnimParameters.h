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
     * Scripts and the Inspector's parameter panel set values ("speed",
     * "isGrounded", "jump"); the controller graph tests them in transition
     * conditions. This type is the whole reason the graph's evaluator can live in
     * this pure library and be tested headless -- keep it free of engine
     * dependencies.
     *
     * THE STORE ITSELF IS UNDECLARED, and stays that way on purpose. A controller
     * asset declares names, types and defaults -- ControllerGraph::parameters, which
     * CAnimator::SeedDeclaredParameters seeds from on every bind and re-save -- but
     * nothing here validates a name against that declaration, so any name can still
     * be set. Two consequences worth knowing:
     *
     *   - A script setting a name the asset does not declare is SILENT. It lands in
     *     this store, no condition ever reads it, and nothing reports it. That is why
     *     the controller editor offers a dropdown over declared names rather than a
     *     text field, and why the Inspector panel lists DECLARATIONS rather than the
     *     entries held here.
     *   - Validating would mean this type holding a pointer back to the graph it
     *     belongs to, which is a coupling the pure layer does not need: the declared
     *     side is already the editor's business, where a mistyped name is visible.
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
        // carries the right mental model. EvaluateController consumes the triggers of
        // the transition it fires, and only that one: at most one per frame, since the
        // first satisfied transition wins.
        void SetTrigger(std::string_view name);
        void ResetTrigger(std::string_view name);
        [[nodiscard]] bool IsTriggerSet(std::string_view name) const;
        [[nodiscard]] bool ConsumeTrigger(std::string_view name);

        // Does this hold `name` at all, whatever its type?
        //
        // Absence is NOT expressible through the getters: each folds a missing name
        // into the caller's fallback, so "absent" and "present and zero/false" are
        // indistinguishable from outside -- and a sentinel fallback cannot separate
        // them either, since a getter returns the fallback for a CROSS-TYPE entry too.
        // Seeding a controller's declared defaults needs exactly that distinction: a
        // default must fill an empty slot and must never overwrite what a script set.
        [[nodiscard]] bool Contains(std::string_view name) const { return Find(name) != nullptr; }

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

            // NOTHING READS THIS -- it is written by FindOrAdd and kept for the
            // debugger alone, where a store of bare hashes is unreadable and a
            // mistyped parameter is otherwise invisible. Worth the string per entry
            // (about five per animator) for exactly that.
            //
            // It also leaves the door open to hardening Find, which compares HASHES
            // ONLY: a collision today returns another parameter's value silently. 64
            // bits make that vanishingly unlikely rather than impossible.
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
