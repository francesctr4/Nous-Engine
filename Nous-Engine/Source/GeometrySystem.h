#pragma once

#include "RendererTypes.inl"

#include "ResourceMesh.h"

struct GeometrySystemConfig
{
    // Max number of geometries that can be loaded at once.
    // NOTE: Should be significantly greater than the number of static meshes because
    // the there can and will be more than one of these per mesh.
    // Take other systems into account as well
    const uint32 maxGeometryCount = 4096;
    const char* DEFAULT_GEOMETRY_NAME = "DefaultGeometry";
};

struct GeometryConfig
{
    std::string name;
    std::string materialPath;

    std::vector<Vertex3D> vertices;
    std::vector<uint32> indices;
};

struct GeometryReference
{
    uint64 referenceCount;
    ResourceMesh geometry;
    bool autoRelease;
};

struct GeometrySystemState
{
    GeometrySystemConfig config;
    ResourceMesh defaultGeometry;

    GeometryReference* registeredGeometries;
};

static GeometrySystemState geometrySystemState;

namespace NOUS_GeometrySystem
{
    bool Initialize();
    void Shutdown();

    ResourceMesh* AcquireByID(uint32 ID);
    ResourceMesh* AcquireFromConfig(GeometryConfig config, bool autoRelease);

    bool CreateDefaultGeometry(GeometrySystemState* state);
    bool CreateGeometry(GeometrySystemState* state, GeometryConfig config, ResourceMesh* geometry);
    void DestroyGeometry(ResourceMesh* geometry);

    void ReleaseGeometry(ResourceMesh* geometry);

    ResourceMesh* GetDefaultGeometry();

    GeometryConfig GeneratePlaneConfig(float width, float height, uint32 xSegmentCount, uint32 ySegmentCount, 
        float tileX, float tileY, std::string name, std::string materialName);
}