#include "EventSystem.h"

#include "MemoryManager.h"

struct registered_event 
{
	void* listener;
	PFN_on_Event callback;
};

struct event_code_entry 
{
	registered_event* events;
};

#define MAX_MESSAGE_CODES 16384

struct event_system_state {
	event_code_entry registered[MAX_MESSAGE_CODES];
};

static bool isInitialized = false;
static event_system_state state;

bool InitalizeEvent()
{
	if (isInitialized) 
	{
		return false;
	}

	isInitialized = false;
	MemoryManager::ZeroMemory(&state, sizeof(state));
	isInitialized = true;

	return true;
}
