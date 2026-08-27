#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"

#include <Logger/Asserts.h>
#include <Logger/Logger.h>

#include <algorithm>

namespace
{
    constexpr size_t SlotOf(ResourceType type)
    {
        return static_cast<size_t>(type);
    }
}

TypeRegistry::~TypeRegistry()
{
    for (auto& slot : m_descriptors)
        slot.reset();
}

void TypeRegistry::Register(TypeDescriptor descriptor)
{
    NOUS_ASSERT_MSG(descriptor.type != ResourceType::UNKNOWN &&
                    descriptor.type != ResourceType::ALL_TYPES,
                    "Registering descriptor with invalid ResourceType");

    const size_t slot = SlotOf(descriptor.type);
    NOUS_ASSERT_MSG(slot < m_descriptors.size(), "ResourceType slot out of range");

    // Runtime-object invariant: a type either has a full runtime lifecycle
    // (createFn + destroyFn + an IResourceImporter) or none of it (pipeline-only,
    // e.g. SCENE). These three must agree — set together via SetImporter<T>() +
    // createFn/destroyFn, or all left null. Catching a half-wired type here at
    // startup beats a silent no-op or a null deref deep in the load path.
    NOUS_ASSERT_MSG((descriptor.createFn != nullptr) == (descriptor.resourceImporter != nullptr),
                    "TypeDescriptor: a type must have both createFn and a resourceImporter "
                    "(runtime resource type) or neither (pipeline-only type like SCENE)");
    NOUS_ASSERT_MSG((descriptor.createFn != nullptr) == (descriptor.destroyFn != nullptr),
                    "TypeDescriptor: createFn and destroyFn must be set together");

    if (m_descriptors[slot])
    {
        NOUS_WARN("TypeRegistry: overwriting descriptor for type %s",
                  descriptor.name ? descriptor.name : "<null>");
    }

    m_descriptors[slot] = std::make_unique<TypeDescriptor>(std::move(descriptor));
}

const TypeDescriptor* TypeRegistry::Get(ResourceType type) const
{
    if (type == ResourceType::UNKNOWN || type == ResourceType::ALL_TYPES)
        return nullptr;

    const size_t slot = SlotOf(type);
    if (slot >= m_descriptors.size())
        return nullptr;

    return m_descriptors[slot].get();
}

TypeDescriptor* TypeRegistry::Get(ResourceType type)
{
    return const_cast<TypeDescriptor*>(
        static_cast<const TypeRegistry*>(this)->Get(type));
}

std::vector<const TypeDescriptor*> TypeRegistry::All() const
{
    std::vector<const TypeDescriptor*> out;
    out.reserve(m_descriptors.size());
    for (const auto& slot : m_descriptors)
    {
        if (slot)
            out.push_back(slot.get());
    }
    return out;
}

std::vector<const TypeDescriptor*> TypeRegistry::SortedByCleanupPriority() const
{
    auto out = All();
    std::stable_sort(out.begin(), out.end(),
        [](const TypeDescriptor* a, const TypeDescriptor* b) {
            return a->cleanupPriority < b->cleanupPriority;
        });
    return out;
}

ResourceType TypeRegistry::TypeFromExtension(const std::string& extension) const
{
    if (extension.empty())
        return ResourceType::UNKNOWN;

    const std::string_view normalized =
        extension[0] == '.' ? std::string_view(extension).substr(1)
                            : std::string_view(extension);

    for (const auto& slot : m_descriptors)
    {
        if (!slot) continue;
        for (const std::string& ext : slot->sourceExtensions)
        {
            if (ext == normalized)
                return slot->type;
        }
    }
    return ResourceType::UNKNOWN;
}
