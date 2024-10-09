#pragma once

#include "Globals.h"

struct EventContext 
{
	union data 
	{
		int64 int64[2];
		uint64 uint64[2];
		float64 float64[2];
		
		int32 int32[4];
		uint32 uint32[4];
		float32 float32[4];

		int16 int16[8];
		uint16 uint16[8];

		int8 int8[16];
		uint8 uint8[16];

		char c[16];
	};

};

// Should return true if handled.
typedef bool (*PFN_on_Event)(uint16 code, void* sender, void* listener_inst, EventContext data);

bool InitalizeEvent();
void ShutdownEvent();

bool RegisterEvent(uint16 code, void* listener_inst, PFN_on_Event on_event);
bool UnregisterEvent(uint16 code, void* listener_inst, PFN_on_Event on_event);
bool FireEvent(uint16 code, void* sender, EventContext context);

enum class EventSystemCode {

	APPLICATION_QUIT = 0x01,
	KEY_PRESSED = 0x02,
	KEY_RELEASED = 0x03,
	BUTTON_PRESSED = 0x04,
	BUTTON_RELEASED = 0x05,
	MOUSE_MOVED = 0x06,
	MOUSE_WHEEL = 0x07,
	WINDOW_RESIZE = 0x08,
	
	MAX_EVENT_CODE = 0xFF,


};