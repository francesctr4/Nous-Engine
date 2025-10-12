#ifndef MODULEFILESYSTEM_H
#define MODULEFILESYSTEM_H

#include "Core/Module.h"
#include <string>

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

    bool CompileShaders();
};

#endif // MODULEFILESYSTEM_H