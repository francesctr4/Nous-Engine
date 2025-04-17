#pragma once

#include "Application.h"

class Module
{
public:

	Module(Application* app);
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