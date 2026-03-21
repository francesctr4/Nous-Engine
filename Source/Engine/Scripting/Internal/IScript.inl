#ifndef NOUS_ENGINE_ISCRIPT_INL
#define NOUS_ENGINE_ISCRIPT_INL

#include <cmath>
#include <cstdint>
#include <vector>
#include <type_traits>

// ---------------------------------------------------------------------------
// ScriptProperty — a named, typed pointer into a script's member variable.
// ---------------------------------------------------------------------------
struct ScriptProperty
{
    enum class Type : uint8_t { Float, Int, Bool, GameObject };

    const char* name;   // display name shown in the Inspector
    Type        type;
    void*       ptr;    // direct pointer into the script's member variable
};

// ---------------------------------------------------------------------------
// SCRIPT_FIELD — declare a field AND auto-register it with the Inspector.
// The label is the exact variable name as written.
//
// Usage (inside a script class body):
//   SCRIPT_FIELD(float, m_speed,  1.0f)
//   SCRIPT_FIELD(int,   m_count,  3)
//   SCRIPT_FIELD(bool,  m_active, true)
// ---------------------------------------------------------------------------
#define SCRIPT_FIELD(T, varName, defaultVal)                                               \
    T varName { defaultVal };                                                               \
    struct _NousSPR_##varName {                                                            \
        _NousSPR_##varName(std::vector<ScriptProperty>& props, T& member) {               \
            ScriptProperty::Type type = ScriptProperty::Type::Float;                       \
            if      constexpr (std::is_same_v<T, float>)                                  \
                type = ScriptProperty::Type::Float;                                        \
            else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>)     \
                type = ScriptProperty::Type::Int;                                          \
            else if constexpr (std::is_same_v<T, bool>)                                   \
                type = ScriptProperty::Type::Bool;                                         \
            props.push_back({ #varName, type, &member });                                  \
        }                                                                                  \
    } _nous_spr_##varName { m_properties, varName };

// ---------------------------------------------------------------------------
// SCRIPT_GAMEOBJECT — declare a uint32_t GameObject ID field that shows as
// a scene-object dropdown in the Inspector.
//
// Usage:
//   SCRIPT_GAMEOBJECT(m_target, 0)   // 0 = no target
//
// In Update(), use the ID like any other binding:
//   if (m_target) Nous_Engine->GameObject->GetPosition(m_target, &x, &y, &z);
// ---------------------------------------------------------------------------
#define SCRIPT_GAMEOBJECT(varName, defaultVal)                                             \
    uint32_t varName { defaultVal };                                                       \
    struct _NousSPR_##varName {                                                            \
        _NousSPR_##varName(std::vector<ScriptProperty>& props, uint32_t& member) {        \
            props.push_back({ #varName, ScriptProperty::Type::GameObject, &member });     \
        }                                                                                  \
    } _nous_spr_##varName { m_properties, varName };

class IScript
{
protected:

    // Prevent creating IScript directly
    IScript() = default;

    // ID of the owning GameObject — set by CScript before calling Awake().
    // Use Nous_Engine->GameObject->SetPosition(m_ownerID, ...) to manipulate self.
    uint32_t m_ownerID = 0;

    // Auto-populated by SCRIPT_FIELD macros at construction time.
    // Do NOT write to this directly — use SCRIPT_FIELD instead.
    std::vector<ScriptProperty> m_properties;

public:

    virtual ~IScript() = default;

    // Must be called instead of delete to ensure deallocation happens in the
    // same DLL heap that allocated this instance (avoids cross-DLL heap mismatch).
    virtual void Destroy() { delete this; }

    // Returns all fields declared with SCRIPT_FIELD.
    // You can still override this manually if you prefer the explicit approach.
    virtual std::vector<ScriptProperty> GetProperties() { return m_properties; }

    // Set/get the owning GameObject ID (called by CScript before Awake)
    void     SetOwnerID(uint32_t id) { m_ownerID = id; }
    uint32_t GetOwnerID()      const { return m_ownerID; }

    // Called once when the script is first attached/loaded
    virtual void Awake() = 0;

    // Called once before the first Update, when the object becomes active
    virtual void Start() = 0;

    // Called every frame
    virtual void Update(float deltaTime) = 0;

    // Called at a fixed timestep (useful for physics updates)
    virtual void FixedUpdate(float fixedDeltaTime) = 0;

    // Called after Update, useful for camera logic or late adjustments
    virtual void LateUpdate(float deltaTime) = 0;

    // Called when the object becomes enabled
    virtual void OnEnable() = 0;

    // Called when the object becomes disabled
    virtual void OnDisable() = 0;

    // Called when the object is destroyed/removed
    virtual void OnDestroy() = 0;
};

#endif // NOUS_ENGINE_ISCRIPT_INL
