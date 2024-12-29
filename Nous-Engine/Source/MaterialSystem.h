#pragma once

#include "RendererTypes.inl"

struct MaterialSystemConfig
{
	const char* DEFAULT_MATERIAL_NAME = "DefaultMaterial";
	const uint32 MAX_MATERIAL_COUNT = 4096;
};

struct MaterialConfig
{
	std::string name;
	bool autoRelease;

	std::string diffuseMapName;
	float4 diffuseColor;
};

struct MaterialSystemState
{
	MaterialSystemConfig config;
	Material defaultMaterial;
	// Array of registered textures.
	Material* registeredMaterials;
	// Hashtable for texture lookups.
	//hashtable registered_texture_table;
};

static MaterialSystemState state;

namespace NOUS_MaterialSystem
{
	bool Initialize();
	void Shutdown();

	bool CreateDefaultMaterials();
	Material* GetDefaultMaterial();
	void DestroyDefaultMaterials();

	Material* AcquireMaterial(const char* path);
	Material* AcquireMaterialFromConfig(MaterialConfig mConfig);
	void ReleaseMaterial(const char* name);
}