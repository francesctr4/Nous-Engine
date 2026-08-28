#ifndef MODULE_H
#define MODULE_H

#include <EngineCore/UpdateStatus.h>
#include <EngineCore/EngineExport.h>

// Forward Declarations
class EventSystem;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

class Module
{
public:

	NOUS_ENGINE_API Module(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);
	NOUS_ENGINE_API virtual ~Module();

	NOUS_ENGINE_API virtual bool Awake();
	NOUS_ENGINE_API virtual bool Start();

	NOUS_ENGINE_API virtual UpdateStatus PreUpdate(float dt);
	NOUS_ENGINE_API virtual UpdateStatus Update(float dt);
	NOUS_ENGINE_API virtual UpdateStatus PostUpdate(float dt);

	NOUS_ENGINE_API virtual bool CleanUp();

protected:

	EventSystem* eventSystem;
	nous::engine::multithreading::NOUS_JobSystem* JobSystem;

};

#endif // MODULE_H