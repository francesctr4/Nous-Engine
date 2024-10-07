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

// Custom Memory Management Macros to monitorize allocations

#define CUSTOM_NEW(name) \
template<typename T, typename... Args> \
T* name(MemoryManager::MemoryTag tag = MemoryManager::MemoryTag::UNKNOWN, Args&&... args) \
{ \
    void* memory = MemoryManager::Allocate(sizeof(T), tag); \
    auto ptr = new(memory) T(std::forward<Args>(args)...); \
    return ptr; \
}

#define CUSTOM_DELETE(name) \
template<typename T> \
void name(T* ptr, MemoryManager::MemoryTag tag = MemoryManager::MemoryTag::UNKNOWN) noexcept \
{ \
    if (ptr != nullptr) \
    { \
        ptr->~T(); \
        MemoryManager::Free(ptr, sizeof(*ptr), tag); \
        ptr = nullptr; \
    } \
}

/**
 * @brief Allocates memory and constructs an object of type T.
 *
 * @param tag: The memory tag used for tracking allocations. Defaults to `UNKNOWN`.
 * @param args: Constructor arguments for the object of type `T`.
 *
 * @return T*: A pointer to the newly constructed object of type `T`.
 */
CUSTOM_NEW(NOUS_NEW)
CUSTOM_DELETE(NOUS_DELETE)