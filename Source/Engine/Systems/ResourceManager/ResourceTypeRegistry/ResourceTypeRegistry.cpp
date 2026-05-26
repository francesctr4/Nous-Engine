#include "Engine/Systems/ResourceManager/ResourceTypeRegistry/ResourceTypeRegistry.h"

#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/Logger/Logger.h"

#include <algorithm>

namespace
{
    ResourceTypeRegistry* g_resourceTypeRegistry = nullptr;

    constexpr size_t SlotOf(ResourceType type)
    {
        return static_cast<size_t>(type);
    }
}

ResourceTypeRegistry::~ResourceTypeRegistry()
{
    for (auto& slot : m_descriptors)
        slot.reset();
}

void ResourceTypeRegistry::Register(ResourceTypeDescriptor descriptor)
{
    NOUS_ASSERT_MSG(descriptor.type != ResourceType::UNKNOWN &&
                    descriptor.type != ResourceType::ALL_TYPES,
                    "Registering descriptor with invalid ResourceType");

    const size_t slot = SlotOf(descriptor.type);
    NOUS_ASSERT_MSG(slot < m_descriptors.size(), "ResourceType slot out of range");

    if (m_descriptors[slot])
    {
        NOUS_WARN("ResourceTypeRegistry: overwriting descriptor for type %s",
                  descriptor.name ? descriptor.name : "<null>");
    }

    m_descriptors[slot] = std::make_unique<ResourceTypeDescriptor>(std::move(descriptor));
}

const ResourceTypeDescriptor* ResourceTypeRegistry::Get(ResourceType type) const
{
    if (type == ResourceType::UNKNOWN || type == ResourceType::ALL_TYPES)
        return nullptr;

    const size_t slot = SlotOf(type);
    if (slot >= m_descriptors.size())
        return nullptr;

    return m_descriptors[slot].get();
}

ResourceTypeDescriptor* ResourceTypeRegistry::Get(ResourceType type)
{
    return const_cast<ResourceTypeDescriptor*>(
        static_cast<const ResourceTypeRegistry*>(this)->Get(type));
}

std::vector<const ResourceTypeDescriptor*> ResourceTypeRegistry::All() const
{
    std::vector<const ResourceTypeDescriptor*> out;
    out.reserve(m_descriptors.size());
    for (const auto& slot : m_descriptors)
    {
        if (slot)
            out.push_back(slot.get());
    }
    return out;
}

std::vector<const ResourceTypeDescriptor*> ResourceTypeRegistry::SortedByCleanupPriority() const
{
    auto out = All();
    std::stable_sort(out.begin(), out.end(),
        [](const ResourceTypeDescriptor* a, const ResourceTypeDescriptor* b) {
            return a->cleanupPriority < b->cleanupPriority;
        });
    return out;
}

ResourceType ResourceTypeRegistry::TypeFromExtension(const std::string& extension) const
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

ResourceTypeRegistry& GetResourceTypeRegistry()
{
    NOUS_ASSERT_MSG(g_resourceTypeRegistry != nullptr,
                    "ResourceTypeRegistry accessed before initialization");
    return *g_resourceTypeRegistry;
}

void SetResourceTypeRegistry(ResourceTypeRegistry* registry)
{
    g_resourceTypeRegistry = registry;
}
