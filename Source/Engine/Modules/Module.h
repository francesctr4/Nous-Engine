#ifndef MODULE_H
#define MODULE_H

#include "Engine/Core/UpdateStatus.h"
#include "Engine/EngineExport.h"

// Forward Declarations
class EventSystem;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

class Module
{
public:

	NOUS_ENGINE_API Module(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem);
	NOUS_ENGINE_API virtual ~Module();

	NOUS_ENGINE_API virtual bool Awake();
	NOUS_ENGINE_API virtual bool Start();

	NOUS_ENGINE_API virtual UpdateStatus PreUpdate(float dt);
	NOUS_ENGINE_API virtual UpdateStatus Update(float dt);
	NOUS_ENGINE_API virtual UpdateStatus PostUpdate(float dt);

	NOUS_ENGINE_API virtual bool CleanUp();

protected:

	EventSystem* eventSystem;
	NOUS_Multithreading::NOUS_JobSystem* JobSystem;

};

#endif // MODULE_H