#pragma once

#include "Application.h"

class Application;

class Module
{
public:

	Module(Application* app, std::string name, bool startEnabled = true);
	virtual ~Module();

	virtual bool Awake();
	virtual bool Start();

	virtual UpdateStatus PreUpdate(float dt);
	virtual UpdateStatus Update(float dt);
	virtual UpdateStatus PostUpdate(float dt);

	virtual bool CleanUp();

	std::string GetName();

	virtual void ReceiveEvent(const Event& event);

public:

	Application* App;
	
private:

	bool enabled;
	std::string name;
	
};