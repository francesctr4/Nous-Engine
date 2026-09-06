#include <ECS/Component/Types/CPrefab/CPrefab.h>

#include <Utils/Serialization/JsonObject.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
    // 16 lowercase hex digits, zero padded, so the string is fixed width and sorts.
    //
    // The hash is deliberately NOT stored as a JSON number: parson holds numbers as
    // double, so a uint64 above 2^53 does not round-trip. The value read back would
    // differ from the value written and every instance would report stale forever,
    // with no error anywhere to say why.
    std::string HashToHex(uint64_t value)
    {
        char buffer[17];
        std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
        return std::string(buffer);
    }

    uint64_t HashFromHex(const std::string& text)
    {
        if (text.empty()) return 0;
        return std::strtoull(text.c_str(), nullptr, 16);
    }
}

JsonObject CPrefab::Serialize() const
{
    JsonObject root;
    root.Set("type",             GetType());
    root.Set("prefabSourcePath", prefabSourcePath);
    root.Set("syncedHash",       HashToHex(syncedHash));
    return root;
}

void CPrefab::Deserialize(const JsonObject& obj)
{
    prefabSourcePath = obj.GetString("prefabSourcePath");
    syncedHash       = HashFromHex(obj.GetString("syncedHash"));
    isStale          = false;   // recomputed on load; never trusted from the file
}
