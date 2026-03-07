#ifndef MODULEFILESYSTEM_H
#define MODULEFILESYSTEM_H

#include "Engine/Modules/Module.h"
#include <string>
#include "Engine/Core/EventSystem/IEventListener.h"

class ModuleFileSystem : public Module, public IEventListener
{
public:

	// Constructor
	ModuleFileSystem(Application* app);

	// Destructor
	virtual ~ModuleFileSystem();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

    void OnEvent(const Event& event) override;

	// -------------------------------------------------------------------- //

	bool CreateLibraryFolder();
	bool ImportDirectory(const std::string& directory);
};

#endif // MODULEFILESYSTEM_H