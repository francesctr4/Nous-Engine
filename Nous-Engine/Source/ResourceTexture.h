#pragma once

#include "Globals.h"
#include "Resource.h"

#include "RendererTypes.inl"

class ResourceTexture : public Resource
{
public:

	// Constructor & Destructor

	ResourceTexture(UID uid = 0);
	virtual ~ResourceTexture();

	// Inherited Functions

	bool LoadInMemory() override;
	bool UnloadFromMemory() override;

public:



};