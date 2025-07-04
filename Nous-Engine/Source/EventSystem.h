#pragma once

#include "Globals.h"

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
	int64 int64[2];
	uint64 uint64[2];
	double float64[2];

	int32 int32[4];
	uint32 uint32[4];
	float float32[4];

	int16 int16[8];
	uint16 uint16[8];

	int8 int8[16];
	uint8 uint8[16];

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