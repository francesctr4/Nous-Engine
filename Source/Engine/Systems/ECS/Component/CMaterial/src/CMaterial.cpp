#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
JSON_Value* CMaterial::Serialize() const {
    // TODO: implement serialization when material properties are needed
    return nullptr;
}

void CMaterial::Deserialize(JSON_Object* obj) {
    // TODO: implement deserialization when material properties are needed
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void CMaterial::OnDestroy()
{
    if (material && material->IsValid()) {
        External->resourceManager->UnloadResource(material->GetUID());
    }
}
