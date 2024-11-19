#pragma once

#include "Module.h"

class ModuleFileSystem : public Module
{
public:

	// Constructor
	ModuleFileSystem(Application* app, std::string name, bool start_enabled = true);

	// Destructor
	virtual ~ModuleFileSystem();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	// -------------------------------------------------------------------------------- //

	std::string GetAbsolutePath(const std::string& path) const;

    bool Exists(const std::string& path) const;

    bool IsDirectory(const std::string& path) const;

    std::string GetFilename(const std::string& path) const;

    std::string GetExtension(const std::string& path) const;

};