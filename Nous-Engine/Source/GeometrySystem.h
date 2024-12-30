#pragma once

#include "RendererTypes.inl"

struct GeometrySystemConfig
{
    // Max number of geometries that can be loaded at once.
    // NOTE: Should be significantly greater than the number of static meshes because
    // the there can and will be more than one of these per mesh.
    // Take other systems into account as well
    uint32 maxGeometryCount = 4096;
    const char* DEFAULT_GEOMETRY_NAME = "DefaultGeometry";
};

struct GeometryConfig
{
    std::string name;
    std::string materialPath;

    std::vector<Vertex> vertices;
    std::vector<uint32> indices;
};

struct GeometryReference
{
    uint64 referenceCount;
    Geometry geometry;
    bool autoRelease;
};

struct GeometrySystemState
{
    GeometrySystemConfig config;
    Geometry defaultGeometry;
    // Array of registered textures.
    GeometryReference* registeredGeometries;
    // Hashtable for texture lookups.
    //hashtable registered_texture_table;
};

static GeometrySystemState geometrySystemState;

namespace NOUS_GeometrySystem
{
    bool Initialize();
    void Shutdown();

    /**
     * @brief Acquires an existing geometry by id.
     *
     * @param id The geometry identifier to acquire by.
     * @return A pointer to the acquired geometry or nullptr if failed.
     */
    Geometry* AcquireByID(uint32 ID);
    /**
     * @brief Registers and acquires a new geometry using the given config.
     *
     * @param config The geometry configuration.
     * @param auto_release Indicates if the acquired geometry should be unloaded when its reference count reaches 0.
     * @return A pointer to the acquired geometry or nullptr if failed.
     */
    Geometry* AcquireFromConfig(GeometryConfig config, bool autoRelease);
    /**
     * @brief Releases a reference to the provided geometry.
     *
     * @param geometry The geometry to be released.
     */
    void ReleaseGeometry(Geometry* geometry);
    /**
     * @brief Obtains a pointer to the default geometry.
     *
     * @return A pointer to the default geometry.
     */
    Geometry* GetDefaultGeometry();

    bool CreateGeometry(GeometrySystemState* state, GeometryConfig config, Geometry* geometry);

    /**
     * @brief Generates configuration for plane geometries given the provided parameters.
     * NOTE: vertex and index arrays are dynamically allocated and should be freed upon object disposal.
     * Thus, this should not be considered production code.
     *
     * @param width The overall width of the plane. Must be non-zero.
     * @param height The overall height of the plane. Must be non-zero.
     * @param x_segment_count The number of segments along the x-axis in the plane. Must be non-zero.
     * @param y_segment_count The number of segments along the y-axis in the plane. Must be non-zero.
     * @param tile_x The number of times the texture should tile across the plane on the x-axis. Must be non-zero.
     * @param tile_y The number of times the texture should tile across the plane on the y-axis. Must be non-zero.
     * @param name The name of the generated geometry.
     * @param material_name The name of the material to be used.
     * @return A geometry configuration which can then be fed into geometry_system_acquire_from_config().
     */
    GeometryConfig GeneratePlaneConfig(float width, float height, u32 xSegmentCount, u32 ySegmentCount, 
        float tileX, float tileY, const char* name, const char* materialName);
}