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

private:

	bool CreateLibraryFolder();

};