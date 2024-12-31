#include "MaterialSystem.h"

#include "MemoryManager.h"

#include "ModuleRenderer3D.h"
#include "TextureSystem.h"
#include "RendererFrontend.h"

#include "JsonFile.h"

#include "ImporterTexture.h"

bool NOUS_MaterialSystem::Initialize()
{
    // Invalidate all geometries in the array.
    uint32 count = state.config.MAX_MATERIAL_COUNT;

    state.registeredMaterials = NOUS_NEW_ARRAY<MaterialReference>(count, MemoryManager::MemoryTag::ARRAY);

    for (uint32 i = 0; i < count; ++i)
    {
        state.registeredMaterials[i].material.ID = INVALID_ID;
        state.registeredMaterials[i].material.internalID = INVALID_ID;
        state.registeredMaterials[i].material.generation = INVALID_ID;
    }

    if (!CreateDefaultMaterials())
    {
        NOUS_FATAL("Failed to create default materials. Application cannot continue.");
        return false;
    }

    return true;
}

void NOUS_MaterialSystem::Shutdown()
{
    NOUS_DELETE_ARRAY(state.registeredMaterials,
        state.config.MAX_MATERIAL_COUNT,
        MemoryManager::MemoryTag::ARRAY);

    DestroyDefaultMaterials();
}

bool NOUS_MaterialSystem::CreateDefaultMaterials()
{
    MemoryManager::ZeroMemory(&state.defaultMaterial, sizeof(Material));

    state.defaultMaterial.ID = INVALID_ID;
    state.defaultMaterial.generation = INVALID_ID;
    state.defaultMaterial.name = state.config.DEFAULT_MATERIAL_NAME;
    state.defaultMaterial.diffuseColor = float4::one;  // white
    state.defaultMaterial.diffuseMap.type = TextureMapType::DIFFUSE;
    state.defaultMaterial.diffuseMap.texture = NOUS_TextureSystem::GetDefaultTexture();

    if (!External->renderer->rendererFrontend->CreateMaterial(&state.defaultMaterial)) 
    {
        NOUS_FATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
        return false;
    }

    return true;
}

Material* NOUS_MaterialSystem::GetDefaultMaterial()
{
    return &state.defaultMaterial;
}

void DestroyMaterial(Material* material)
{
    NOUS_TRACE("Destroying material '%s'...", material->name);

    // Release texture references.
    if (material->diffuseMap.texture)
    {
        NOUS_TextureSystem::ReleaseTexture(material->diffuseMap.texture);
    }

    // Release renderer resources.
    External->renderer->rendererFrontend->DestroyMaterial(material);

    // Zero it out, invalidate IDs.
    MemoryManager::ZeroMemory(material, sizeof(material));

    material->ID = INVALID_ID;
    material->generation = INVALID_ID;
    material->internalID = INVALID_ID;
}

void NOUS_MaterialSystem::DestroyDefaultMaterials()
{
    // Destroy the default material.
    DestroyMaterial(&state.defaultMaterial);
}

Material* NOUS_MaterialSystem::AcquireMaterial(const char* path)
{
    // Load the JSON file
    JsonFile jsonFile;
    if (!jsonFile.LoadFromFile(path)) {
        // Handle error: Unable to load the file
        return nullptr;
    }

    // Create a new Material object
    Material* material = NOUS_NEW<Material>(MemoryManager::MemoryTag::MATERIAL_INSTANCE);

    if (!jsonFile.GetValue("name", material->name)) {
        // Handle missing or invalid "name" field
        delete material;
        return nullptr;
    }

    if (!jsonFile.GetValue("diffuse_color", material->diffuseColor)) {
        // Handle missing or invalid "diffuse_color" field
        delete material;
        return nullptr;
    }

    std::string texPath;
    if (!jsonFile.GetValue("diffuse_map_name", texPath)) {
        // Handle missing or invalid "diffuse_map_name" field
        delete material;
        return nullptr;
    }

    material->diffuseMap.texture = NOUS_NEW<Texture>(MemoryManager::MemoryTag::TEXTURE);

    ImporterTexture::Import(texPath.c_str(), material->diffuseMap.texture);

    //material->diffuseMap.texture = NOUS_TextureSystem::GetDefaultTexture();
    // Create a new Material object
    //material->diffuseMap.texture = NOUS_TextureSystem::AcquireTexture(texPath.c_str(), true);

    state.registeredMaterials[0].material.ID = 0;
    state.registeredMaterials[0].autoRelease = true;

    // Return the populated material
    return material;
}

Material* NOUS_MaterialSystem::AcquireMaterialFromConfig(MaterialConfig mConfig)
{
    // Return default material.
    if (mConfig.name == state.config.DEFAULT_MATERIAL_NAME) 
    {
        return &state.defaultMaterial;
    }
    //material_reference ref;
    //if (state_ptr && hashtable_get(&state_ptr->registered_material_table, config.name, &ref)) {
    //    // This can only be changed the first time a material is loaded.
    //    if (ref.reference_count == 0) {
    //        ref.auto_release = config.auto_release;
    //    }
    //    ref.reference_count++;
    //    if (ref.handle == INVALID_ID) {
    //        // This means no material exists here. Find a free index first.
    //        u32 count = state_ptr->config.max_material_count;
    //        material* m = 0;
    //        for (u32 i = 0; i < count; ++i) {
    //            if (state_ptr->registered_materials[i].id == INVALID_ID) {
    //                // A free slot has been found. Use its index as the handle.
    //                ref.handle = i;
    //                m = &state_ptr->registered_materials[i];
    //                break;
    //            }
    //        }
    //        // Make sure an empty slot was actually found.
    //        if (!m || ref.handle == INVALID_ID) {
    //            KFATAL("material_system_acquire - Material system cannot hold anymore materials. Adjust configuration to allow more.");
    //            return 0;
    //        }
    //        // Create new material.
    //        if (!load_material(config, m)) {
    //            KERROR("Failed to load material '%s'.", config.name);
    //            return 0;
    //        }
    //        if (m->generation == INVALID_ID) {
    //            m->generation = 0;
    //        }
    //        else {
    //            m->generation++;
    //        }
    //        // Also use the handle as the material id.
    //        m->id = ref.handle;
    //        KTRACE("Material '%s' does not yet exist. Created, and ref_count is now %i.", config.name, ref.reference_count);
    //    }
    //    else {
    //        KTRACE("Material '%s' already exists, ref_count increased to %i.", config.name, ref.reference_count);
    //    }
    //    // Update the entry.
    //    hashtable_set(&state_ptr->registered_material_table, config.name, &ref);
    //    return &state_ptr->registered_materials[ref.handle];
    //}
    //// NOTE: This would only happen in the event something went wrong with the state.
    //KERROR("material_system_acquire_from_config failed to acquire material '%s'. Null pointer will be returned.", config.name);
    //return 0;
}

void NOUS_MaterialSystem::ReleaseMaterial(Material* material)
{
    if (material && material->ID != INVALID_ID)
    {
        MaterialReference* ref = &state.registeredMaterials[material->ID];
        uint32 ID = material->ID;

        if (ref->material.ID == ID)
        {
            if (ref->referenceCount > 0)
            {
                ref->referenceCount--;
            }

            // Also blanks out the geometry ID.
            if (ref->referenceCount < 1 && ref->autoRelease)
            {
                DestroyMaterial(&ref->material);

                ref->referenceCount = 0;
                ref->autoRelease = false;
            }
        }
        else
        {
            NOUS_FATAL("Material ID mismatch. Check registration logic, as this should never occur.");
        }

        return;
    }

    NOUS_WARN("NOUS_GeometrySystem::ReleaseMaterial() cannot release invalid material id. Nothing was done.");
}
