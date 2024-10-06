#pragma once

#include "Globals.h"

namespace MemoryManager 
{
	enum class MemoryTag
	{
		UNKNOWN = 0,

		ARRAY,
		DARRAY,
		DICT,
		RING_QUEUE,
		BST,
		STRING,
		APPLICATION,
		JOB,
		TEXTURE,
		MATERIAL_INSTANCE,
		RENDERER,
		GAME,
		TRANSFORM,
		ENTITY,
		ENTITY_NODE,
		SCENE,

		MAX
	};

	void InitializeMemory();

	void ShutdownMemory();

	void* Allocate(uint64 size, MemoryTag tag);

	void Free(void* block, uint64 size, MemoryTag tag);

	void* ZeroMemory(void* block, uint64 size);

	void* CopyMemory(void* destination, const void* source, uint64 size);

	void* SetMemory(void* destination, int32 value, uint64 size);

	char* GetMemoryUsageStats();

}