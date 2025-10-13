#ifndef MODULE_H
#define MODULE_H

#include "UpdateStatus.h"

struct Event;
class Application;

class Module
{
public:

	explicit Module(Application* app);
	virtual ~Module();

	virtual bool Awake();
	virtual bool Start();

	virtual UpdateStatus PreUpdate(float dt);
	virtual UpdateStatus Update(float dt);
	virtual UpdateStatus PostUpdate(float dt);

	virtual bool CleanUp();

	virtual void ReceiveEvent(const Event& event);

public:

	Application* App;
	
};

#endif // MODULE_H