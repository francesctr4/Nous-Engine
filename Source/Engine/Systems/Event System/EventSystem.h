#ifndef EVENTSYSTEM_H
#define EVENTSYSTEM_H

#include "Engine/Core/Globals.h"
#include <cstring>

enum class EventType
{
	TEST,
	KEY_PRESSED,
	WINDOW_RESIZED,
	SWAP_TEXTURE,
	DROP_FILE
};

union EventContext
{
	int64 _i64[2];
	uint64 _u64[2];
	double _f64[2];

	int32 _i32[4];
	uint32 _u32[4];
	float _f32[4];

	int16 _i16[8];
	uint16 _u16[8];

	int8 _i8[16];
	uint8 _u8[16];

	const char* c;
};

struct Event 
{
	EventType type;
	EventContext context;
	
	Event(EventType type) : type(type) 
	{
		memset(&context, 0, sizeof(EventContext));
	}

	Event(EventType type, const EventContext& ctx) : type(type), context(ctx) {}
};

#endif // EVENTSYSTEM_H