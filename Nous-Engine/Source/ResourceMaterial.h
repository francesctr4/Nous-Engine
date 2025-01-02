#pragma once

#include "Globals.h"
#include "Resource.h"

#include "RendererTypes.inl"

class ResourceMaterial : public Resource
{
public:

	// Constructor & Destructor

	ResourceMaterial(UID uid = 0);
	virtual ~ResourceMaterial();

	// Inherited Functions

	bool LoadInMemory() override;
	bool UnloadFromMemory() override;

public:



};