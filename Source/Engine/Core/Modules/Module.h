#ifndef MODULE_H
#define MODULE_H

#include "Engine/Core/UpdateStatus.h"
#include "Engine/Core/EngineExport.h"

struct Event;
class Application;

class Module
{
public:

	NOUS_ENGINE_API explicit Module(Application* app);
	NOUS_ENGINE_API virtual ~Module();

	NOUS_ENGINE_API virtual bool Awake();
	NOUS_ENGINE_API virtual bool Start();

	NOUS_ENGINE_API virtual UpdateStatus PreUpdate(float dt);
	NOUS_ENGINE_API virtual UpdateStatus Update(float dt);
	NOUS_ENGINE_API virtual UpdateStatus PostUpdate(float dt);

	NOUS_ENGINE_API virtual bool CleanUp();

public:

	Application* App;
	
};

#endif // MODULE_H