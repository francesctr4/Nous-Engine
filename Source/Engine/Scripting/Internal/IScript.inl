#ifndef NOUS_ENGINE_ISCRIPT_INL
#define NOUS_ENGINE_ISCRIPT_INL

class IScript
{
protected:

    // Prevent creating IScript directly
    IScript() = default;

public:

    virtual ~IScript() = default;

    // Must be called instead of delete to ensure deallocation happens in the
    // same DLL heap that allocated this instance (avoids cross-DLL heap mismatch).
    virtual void Destroy() { delete this; }

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
