#ifndef NOUS_ENGINE_ISCRIPT_INL
#define NOUS_ENGINE_ISCRIPT_INL

#include <cstdint>

class IScript
{
protected:

    // Prevent creating IScript directly
    IScript() = default;

    // ID of the owning GameObject — set by CScript before calling Awake().
    // Use Nous_Engine->GameObject->SetPosition(m_ownerID, ...) to manipulate self.
    uint32_t m_ownerID = 0;

public:

    virtual ~IScript() = default;

    // Must be called instead of delete to ensure deallocation happens in the
    // same DLL heap that allocated this instance (avoids cross-DLL heap mismatch).
    virtual void Destroy() { delete this; }

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
