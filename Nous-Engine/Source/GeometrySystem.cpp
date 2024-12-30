#include "GeometrySystem.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"

#include "MaterialSystem.h"

#include "MemoryManager.h"

bool NOUS_GeometrySystem::Initialize()
{
    // Invalidate all geometries in the array.
    uint32 count = geometrySystemState.config.maxGeometryCount;

    geometrySystemState.registeredGeometries = NOUS_NEW_ARRAY<GeometryReference>(count, MemoryManager::MemoryTag::ARRAY);

    for (uint32 i = 0; i < count; ++i) 
    {
        geometrySystemState.registeredGeometries[i].geometry.ID = INVALID_ID;
        geometrySystemState.registeredGeometries[i].geometry.internalID = INVALID_ID;
        geometrySystemState.registeredGeometries[i].geometry.generation = INVALID_ID;
    }

	return true;
}

void NOUS_GeometrySystem::Shutdown()
{
    NOUS_DELETE_ARRAY(geometrySystemState.registeredGeometries, 
        geometrySystemState.config.maxGeometryCount, 
        MemoryManager::MemoryTag::ARRAY);
}

Geometry* NOUS_GeometrySystem::AcquireByID(uint32 ID)
{
	return nullptr;
}

Geometry* NOUS_GeometrySystem::AcquireFromConfig(GeometryConfig config, bool autoRelease)
{
    Geometry* geometry = nullptr;

    for (uint32 i = 0; i < geometrySystemState.config.maxGeometryCount; ++i) 
    {
        if (geometrySystemState.registeredGeometries[i].geometry.ID == INVALID_ID)
        {
            // Found empty slot.
            geometrySystemState.registeredGeometries[i].autoRelease = autoRelease;
            geometrySystemState.registeredGeometries[i].referenceCount = 1;

            geometry = &geometrySystemState.registeredGeometries[i].geometry;
            geometry->ID = i;

            break;
        }
    }

    if (!geometry) 
    {
        NOUS_ERROR("Unable to obtain free slot for geometry. Adjust configuration to allow more space. Returning nullptr.");
        return nullptr;
    }

    if (!CreateGeometry(&geometrySystemState, config, geometry)) 
    {
        NOUS_ERROR("Failed to create geometry. Returning nullptr.");
        return nullptr;
    }

    return geometry;
}

void NOUS_GeometrySystem::ReleaseGeometry(Geometry* geometry)
{

}

Geometry* NOUS_GeometrySystem::GetDefaultGeometry()
{
	return nullptr;
}

bool NOUS_GeometrySystem::CreateGeometry(GeometrySystemState* state, GeometryConfig config, Geometry* geometry)
{
    // Send the geometry off to the renderer to be uploaded to the GPU.
    if (!External->renderer->rendererFrontend->CreateGeometry(
        config.vertices.size(), config.vertices.data(), 
        config.indices.size(), config.indices.data(), 
        geometry))
    {
        // Invalidate the entry.
        //state->registered_geometries[g->id].reference_count = 0;
        //state->registered_geometries[g->id].auto_release = false;

        geometry->ID = INVALID_ID;
        geometry->generation = INVALID_ID;
        geometry->internalID = INVALID_ID;

        return false;
    }

    // Acquire the material
    if (!config.materialPath.empty())
    {
        geometry->material = NOUS_MaterialSystem::AcquireMaterial(config.materialPath.c_str());

        if (!geometry->material)
        {
            geometry->material = NOUS_MaterialSystem::GetDefaultMaterial();
        }
    }

    return true;
}

GeometryConfig NOUS_GeometrySystem::GeneratePlaneConfig(float width, float height, u32 xSegmentCount, u32 ySegmentCount, float tileX, float tileY, const char* name, const char* materialName)
{
	return GeometryConfig();
}
