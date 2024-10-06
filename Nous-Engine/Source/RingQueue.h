#pragma once

#include "Globals.h"

// Ring Queue

class RingQueue 
{
public:

	RingQueue(uint32 stride, uint32 capacity, void* memory);
	~RingQueue();

	bool Enqueue(void* value);
	bool Dequeue();
	bool Peek();

private:

	uint32 length;
	uint32 stride;
	uint32 capacity;

	void* block;
	bool ownsMemory;

	int32 head;
	int32 tail;
};