#pragma once

#include "Module.h"

class ModuleFileSystem : public Module
{
public:

	// Constructor
	ModuleFileSystem(Application* app);

	// Destructor
	virtual ~ModuleFileSystem();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	bool CreateLibraryFolder();
	bool ImportDirectory(const std::string& directory);

};