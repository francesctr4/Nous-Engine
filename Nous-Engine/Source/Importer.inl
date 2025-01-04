#pragma once

#include "Globals.h"
#include "Resource.h"

static class Importer 
{
	virtual bool Import(const std::string& assetsPath) = 0;
	virtual bool Save(const std::string& libraryPath, const Resource*& inResource) = 0;

	virtual bool Load(const std::string& libraryPath, Resource* outResource) = 0;
};