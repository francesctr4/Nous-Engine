#include <Scripting/Internal/IScript.inl>
#include <Scripting/Internal/ScriptRegistry.inl>
#include <Scripting/EngineAPI/EngineAPI.h>
#include <Scripting/EngineAPI/Bindings/ScriptBindings.h>

// Include the helper class defined in the same folder.
// This is the whole point of the test: a script that pulls in a .h/.cpp pair.
#include "SpinnerComponent.h"

class SpinnerScript : public IScript
{
public:

    SpinnerScript()  = default;
    ~SpinnerScript() override = default;

    void Awake() override {}

    void Start() override
    {
        m_spinner.speedDegPerSec = m_speedDegPerSec;
        m_spinner.axisX          = m_axisX;
        m_spinner.axisY          = m_axisY;
        m_spinner.axisZ          = m_axisZ;
    }

    void Update(float deltaTime) override
    {
        // Keep inspector-editable fields in sync with the component.
        m_spinner.speedDegPerSec = m_speedDegPerSec;
        m_spinner.axisX          = m_axisX;
        m_spinner.axisY          = m_axisY;
        m_spinner.axisZ          = m_axisZ;

        m_spinner.Tick(deltaTime);

        float x, y, z;
        m_spinner.GetCurrentRotation(x, y, z);
        Nous_Engine->GameObject->SetRotation(m_ownerID, x, y, z);
    }

    void FixedUpdate(float) override {}
    void LateUpdate(float)  override {}
    void OnEnable()         override {}
    void OnDisable()        override {}
    void OnDestroy()        override {}

private:

    SCRIPT_FIELD(float, m_speedDegPerSec, 90.0f)
    SCRIPT_FIELD(float, m_axisX,           0.0f)
    SCRIPT_FIELD(float, m_axisY,           1.0f)
    SCRIPT_FIELD(float, m_axisZ,           0.0f)

    SpinnerComponent m_spinner;
};

REGISTER_SCRIPT(SpinnerScript)
