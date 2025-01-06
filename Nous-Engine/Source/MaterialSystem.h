#pragma once

#include "RendererTypes.inl"

#include "ResourceMaterial.h"

namespace NOUS_MaterialSystem
{
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

	struct MaterialReference 
	{
		uint64 referenceCount;
		ResourceMaterial material;
		bool autoRelease;
	};

	struct MaterialSystemState
	{
		MaterialSystemConfig config;
		ResourceMaterial defaultMaterial;
		// Array of registered textures.
		MaterialReference* registeredMaterials;
		// Hashtable for texture lookups.
		//hashtable registered_texture_table;
	};

	static MaterialSystemState state;

	bool Initialize();
	void Shutdown();

	bool CreateDefaultMaterials();
	ResourceMaterial* GetDefaultMaterial();
	void DestroyDefaultMaterials();

	ResourceMaterial* AcquireMaterial(const char* path);
	ResourceMaterial* AcquireMaterialFromConfig(MaterialConfig mConfig);
	void ReleaseMaterial(ResourceMaterial* name);
}