#pragma once

#include "Globals.h"
#include "Vertex.inl"

struct Texture 
{
    uint32 ID;
    uint32 width;
    uint32 height;
    uint8 channelCount;
    bool hasTransparency;
    uint32 generation;
    std::string name;
    void* internalData;
};

enum class TextureMapType
{
    UNKNOWN = -1,
    DIFFUSE
};

struct TextureMap 
{
    TextureMapType type;
    Texture* texture;
};

struct Material 
{
    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::string name;

    TextureMap diffuseMap;
    float4 diffuseColor;
};

/**
 * @brief Represents actual geometry in the world.
 * Typically (but not always, depending on use) paired with a material.
 */
struct Geometry 
{
    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::string name;

    Material* material;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;
};