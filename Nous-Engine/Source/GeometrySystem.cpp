#include "GeometrySystem.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"

#include "MaterialSystem.h"

#include "MemoryManager.h"
#include "ResourceMesh.h"

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

    if (!CreateDefaultGeometry(&geometrySystemState)) 
    {
        NOUS_FATAL("Failed to create default geometry. Application cannot continue.");
        return false;
    }

	return true;
}

void NOUS_GeometrySystem::Shutdown()
{
    NOUS_DELETE_ARRAY(geometrySystemState.registeredGeometries, 
        geometrySystemState.config.maxGeometryCount, 
        MemoryManager::MemoryTag::ARRAY);
}

ResourceMesh* NOUS_GeometrySystem::AcquireByID(uint32 ID)
{
    if (ID != INVALID_ID && geometrySystemState.registeredGeometries[ID].geometry.ID != INVALID_ID)
    {
        geometrySystemState.registeredGeometries[ID].referenceCount++;

        return &geometrySystemState.registeredGeometries[ID].geometry;
    }

    // NOTE: Should return default geometry instead?
    NOUS_ERROR("NOUS_GeometrySystem::AcquireByID() cannot load invalid geometry ID. Returning nullptr.");

    return nullptr;
}

ResourceMesh* NOUS_GeometrySystem::AcquireFromConfig(GeometryConfig config, bool autoRelease)
{
    ResourceMesh* geometry = nullptr;

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

void NOUS_GeometrySystem::ReleaseGeometry(ResourceMesh* geometry)
{
    if (geometry && geometry->ID != INVALID_ID) 
    {
        GeometryReference* ref = &geometrySystemState.registeredGeometries[geometry->ID];
        uint32 ID = geometry->ID;

        if (ref->geometry.ID == ID) 
        {
            if (ref->referenceCount > 0) 
            {
                ref->referenceCount--;
            }

            // Also blanks out the geometry ID.
            if (ref->referenceCount < 1 && ref->autoRelease) 
            {
                DestroyGeometry(&ref->geometry);

                ref->referenceCount = 0;
                ref->autoRelease = false;
            }
        }
        else 
        {
            NOUS_FATAL("Geometry ID mismatch. Check registration logic, as this should never occur.");
        }

        return;
    }

    NOUS_WARN("NOUS_GeometrySystem::ReleaseGeometry() cannot release invalid geometry id. Nothing was done.");
}

ResourceMesh* NOUS_GeometrySystem::GetDefaultGeometry()
{
    return &geometrySystemState.defaultGeometry;
}

bool NOUS_GeometrySystem::CreateGeometry(GeometrySystemState* state, GeometryConfig config, ResourceMesh* geometry)
{
    //// Send the geometry off to the renderer to be uploaded to the GPU.
    //if (!External->renderer->rendererFrontend->CreateGeometry(
    //    config.vertices.size(), config.vertices.data(), 
    //    config.indices.size(), config.indices.data(), 
    //    geometry))
    //{
    //    // Invalidate the entry.
    //    state->registeredGeometries[geometry->ID].referenceCount = 0;
    //    state->registeredGeometries[geometry->ID].autoRelease = false;

    //    geometry->ID = INVALID_ID;
    //    geometry->generation = INVALID_ID;
    //    geometry->internalID = INVALID_ID;

    //    return false;
    //}

    //// Acquire the material
    //if (!config.materialPath.empty())
    //{
    //    geometry->material = NOUS_MaterialSystem::AcquireMaterial(config.materialPath.c_str());

    //    if (!geometry->material)
    //    {
    //        geometry->material = NOUS_MaterialSystem::GetDefaultMaterial();
    //    }
    //}

    return true;
}

void NOUS_GeometrySystem::DestroyGeometry(ResourceMesh* geometry)
{
    //External->renderer->rendererFrontend->DestroyGeometry(geometry);

    //geometry->internalID = INVALID_ID;
    //geometry->generation = INVALID_ID;
    //geometry->ID = INVALID_ID;
    //
    //geometry->name.clear();

    //// Release the material.
    //if (geometry->material && !geometry->material->name.empty())
    //{
    //    NOUS_MaterialSystem::ReleaseMaterial(geometry->material);
    //    geometry->material = nullptr;
    //}
}

bool NOUS_GeometrySystem::CreateDefaultGeometry(GeometrySystemState* state)
{
    std::array<Vertex, 4> vertices;
    MemoryManager::ZeroMemory(vertices.data(), sizeof(vertices));

    const float f = 10.0f;

    vertices[0].position.x = -0.5 * f;  // 0    3
    vertices[0].position.y = -0.5 * f;  //

    vertices[0].texCoord.x = 0.0f;      //
    vertices[0].texCoord.y = 0.0f;      // 2    1

    vertices[1].position.y = 0.5 * f;
    vertices[1].position.x = 0.5 * f;

    vertices[1].texCoord.x = 1.0f;
    vertices[1].texCoord.y = 1.0f;

    vertices[2].position.x = -0.5 * f;
    vertices[2].position.y = 0.5 * f;

    vertices[2].texCoord.x = 0.0f;
    vertices[2].texCoord.y = 1.0f;

    vertices[3].position.x = 0.5 * f;
    vertices[3].position.y = -0.5 * f;

    vertices[3].texCoord.x = 1.0f;
    vertices[3].texCoord.y = 0.0f;

    std::array<uint32, 6> indices = { 0, 1, 2, 0, 3, 1 };

    // Send the geometry off to the renderer to be uploaded to the GPU.
    //if (!External->renderer->rendererFrontend->CreateGeometry(vertices.size(), vertices.data(), indices.size(), indices.data(), &state->defaultGeometry))
    //{
    //    NOUS_FATAL("Failed to create default geometry. Application cannot continue.");
    //    return false;
    //}

    // Acquire the default material.
    state->defaultGeometry.material = NOUS_MaterialSystem::GetDefaultMaterial();

    return true;
}

GeometryConfig NOUS_GeometrySystem::GeneratePlaneConfig(float width, float height, uint32 xSegmentCount, uint32 ySegmentCount, float tileX, float tileY, std::string name, std::string materialName)
{
    if (width == 0) 
    {
        NOUS_WARN("Width must be nonzero. Defaulting to one.");
        width = 1.0f;
    }

    if (height == 0) 
    {
        NOUS_WARN("Height must be nonzero. Defaulting to one.");
        height = 1.0f;
    }

    if (xSegmentCount < 1)
    {
        NOUS_WARN("xSegmentCount must be a positive number. Defaulting to one.");
        xSegmentCount = 1;
    }

    if (ySegmentCount < 1)
    {
        NOUS_WARN("ySegmentCount must be a positive number. Defaulting to one.");
        ySegmentCount = 1;
    }

    if (tileX == 0) 
    {
        NOUS_WARN("tileX must be nonzero. Defaulting to one.");
        tileX = 1.0f;
    }

    if (tileY == 0) 
    {
        NOUS_WARN("tileY must be nonzero. Defaulting to one.");
        tileY = 1.0f;
    }

    GeometryConfig config;

    uint32 vertexCount = xSegmentCount * ySegmentCount * 4;  // 4 verts per segment
    config.vertices.resize(vertexCount);

    uint32 indexCount = xSegmentCount * ySegmentCount * 6;  // 6 indices per segment
    config.indices.resize(indexCount);

    // TODO: This generates extra vertices, but we can always deduplicate them later.
    float segWidth = width / (float)xSegmentCount;
    float segHeight = height / (float)ySegmentCount;
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;

    for (uint32 y = 0; y < ySegmentCount; ++y)
    {
        for (uint32 x = 0; x < xSegmentCount; ++x)
        {
            // Generate vertices
            float minX = (x * segWidth) - halfWidth;
            float minY = (y * segHeight) - halfHeight;

            float maxX = minX + segWidth;
            float maxY = minY + segHeight;

            float minUVx = ((float)x / (float)xSegmentCount) * tileX;
            float minUVy = ((float)y / (float)ySegmentCount) * tileY;

            float maxUVx = (((float)x + (float)1) / (float)xSegmentCount) * tileX;
            float maxUVy = (((float)y + (float)1) / (float)ySegmentCount) * tileY;

            uint32 vOffset = ((y * xSegmentCount) + x) * 4;

            Vertex* v0 = &config.vertices[vOffset + 0];
            Vertex* v1 = &config.vertices[vOffset + 1];
            Vertex* v2 = &config.vertices[vOffset + 2];
            Vertex* v3 = &config.vertices[vOffset + 3];

            v0->position.x = minX;
            v0->position.y = minY;
            v0->texCoord.x = minUVx;
            v0->texCoord.y = minUVy;

            v1->position.x = maxX;
            v1->position.y = maxY;
            v1->texCoord.x = maxUVx;
            v1->texCoord.y = maxUVy;

            v2->position.x = minX;
            v2->position.y = maxY;
            v2->texCoord.x = minUVx;
            v2->texCoord.y = maxUVy;

            v3->position.x = maxX;
            v3->position.y = minY;
            v3->texCoord.x = maxUVx;
            v3->texCoord.y = minUVy;

            // Generate indices
            uint32 iOffset = ((y * xSegmentCount) + x) * 6;

            config.indices[iOffset + 0] = vOffset + 0;
            config.indices[iOffset + 1] = vOffset + 1;
            config.indices[iOffset + 2] = vOffset + 2;
            config.indices[iOffset + 3] = vOffset + 0;
            config.indices[iOffset + 4] = vOffset + 3;
            config.indices[iOffset + 5] = vOffset + 1;
        }
    }

    if (!name.empty()) 
    {
        config.name = name;
    }
    else 
    {
        config.name = geometrySystemState.config.DEFAULT_GEOMETRY_NAME;
    }

    if (!materialName.empty())
    {
        config.materialPath = materialName;
    }
    else
    {
        config.materialPath = NOUS_MaterialSystem::state.config.DEFAULT_MATERIAL_NAME;
    }

    return config;
}
