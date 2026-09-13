#include <AnimationSystem/AnimParameters.h>

namespace nous::engine::animation_system
{
    namespace
    {
        // FNV-1a 64, matching HashBoneNames. 32 bits would do for five names, but a
        // collision here silently returns another parameter's value and 64 is free.
        uint64_t HashName(std::string_view name)
        {
            constexpr uint64_t c_offsetBasis = 14695981039346656037ull;
            constexpr uint64_t c_prime       = 1099511628211ull;

            uint64_t hash = c_offsetBasis;
            for (const char ch : name)
            {
                hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
                hash *= c_prime;
            }
            return hash;
        }
    }

    const AnimParameters::Entry* AnimParameters::Find(const std::string_view name) const
    {
        const uint64_t hash = HashName(name);
        for (const Entry& e : m_entries)
            if (e.hash == hash)
                return &e;
        return nullptr;
    }

    AnimParameters::Entry* AnimParameters::Find(const std::string_view name)
    {
        const uint64_t hash = HashName(name);
        for (Entry& e : m_entries)
            if (e.hash == hash)
                return &e;
        return nullptr;
    }

    AnimParameters::Entry& AnimParameters::FindOrAdd(const std::string_view name,
                                                     const AnimParamType    type)
    {
        if (Entry* existing = Find(name))
        {
            // Re-setting a name under a different type retypes it rather than adding a
            // second entry: two entries with one hash would make Find's result depend
            // on insertion order.
            existing->type = type;
            return *existing;
        }

        Entry entry;
        entry.hash = HashName(name);
        entry.type = type;
        entry.name = std::string(name);
        m_entries.push_back(std::move(entry));
        return m_entries.back();
    }

    void AnimParameters::SetFloat(const std::string_view name, const float value)
    {
        FindOrAdd(name, AnimParamType::Float).value = value;
    }

    float AnimParameters::GetFloat(const std::string_view name, const float fallback) const
    {
        const Entry* e = Find(name);
        return (e && e->type == AnimParamType::Float) ? e->value : fallback;
    }

    void AnimParameters::SetBool(const std::string_view name, const bool value)
    {
        FindOrAdd(name, AnimParamType::Bool).value = value ? 1.0f : 0.0f;
    }

    bool AnimParameters::GetBool(const std::string_view name, const bool fallback) const
    {
        const Entry* e = Find(name);
        return (e && e->type == AnimParamType::Bool) ? (e->value != 0.0f) : fallback;
    }

    void AnimParameters::SetTrigger(const std::string_view name)
    {
        FindOrAdd(name, AnimParamType::Trigger).value = 1.0f;
    }

    void AnimParameters::ResetTrigger(const std::string_view name)
    {
        // Deliberately does NOT create the entry: resetting a trigger that was never
        // set is a no-op, not a declaration.
        if (Entry* e = Find(name); e && e->type == AnimParamType::Trigger)
            e->value = 0.0f;
    }

    bool AnimParameters::IsTriggerSet(const std::string_view name) const
    {
        const Entry* e = Find(name);
        return e && e->type == AnimParamType::Trigger && e->value != 0.0f;
    }

    bool AnimParameters::ConsumeTrigger(const std::string_view name)
    {
        Entry* e = Find(name);
        if (!e || e->type != AnimParamType::Trigger || e->value == 0.0f)
            return false;

        e->value = 0.0f;
        return true;
    }
}
