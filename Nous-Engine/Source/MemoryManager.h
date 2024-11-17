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
		INPUT,
		LINEAR_ALLOCATOR,

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

	uint64 GetMemoryAllocationCount();
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

/**
 * @brief Allocates memory and constructs an object of type T.
 *
 * @param tag: The memory tag used for tracking allocations. Defaults to `UNKNOWN`.
 * @param args: Constructor arguments for the object of type `T`.
 *
 * @return T*: A pointer to the newly constructed object of type `T`.
 */
CUSTOM_NEW(NOUS_NEW)

#define CUSTOM_NEW_ARRAY(name) \
template<typename T> \
T* name(size_t count, MemoryManager::MemoryTag tag = MemoryManager::MemoryTag::UNKNOWN) \
{ \
    void* memory = MemoryManager::Allocate(sizeof(T) * count, tag); \
    auto ptr = static_cast<T*>(memory); \
    for (size_t i = 0; i < count; ++i) \
    { \
        new(&ptr[i]) T(); \
    } \
    return ptr; \
}

/**
* @brief Allocates memory for an array of objects of type T and constructs them.
*
* @param count: The number of elements to allocate.
* @param tag: The memory tag used for tracking allocations. Defaults to `UNKNOWN`.
*
* @return T*: A pointer to the first element in the newly constructed array.
*/
CUSTOM_NEW_ARRAY(NOUS_NEW_ARRAY)

#define CUSTOM_DELETE(name) \
template<typename T> \
void name(T* ptr, MemoryManager::MemoryTag tag = MemoryManager::MemoryTag::UNKNOWN) \
{ \
    if (ptr != nullptr) \
    { \
        ptr->~T(); \
        MemoryManager::Free(ptr, sizeof(*ptr), tag); \
        ptr = nullptr; \
    } \
}

/**
 * @brief Destructs an object of type T and deallocates the memory.
 *
 * @param ptr: A pointer to the object to be deleted.
 * @param tag: The memory tag used for tracking deallocations. Defaults to `UNKNOWN`.
 */
CUSTOM_DELETE(NOUS_DELETE)

#define CUSTOM_DELETE_ARRAY(name) \
template<typename T> \
void name(T* ptr, size_t count, MemoryManager::MemoryTag tag = MemoryManager::MemoryTag::UNKNOWN) \
{ \
    if (ptr != nullptr) \
    { \
        for (size_t i = 0; i < count; ++i) \
        { \
            ptr[i].~T(); \
        } \
        MemoryManager::Free(ptr, sizeof(T) * count, tag); \
        ptr = nullptr; \
    } \
}

/**
* @brief Destructs an array of objects of type T and deallocates the memory.
*
* @param ptr: A pointer to the array.
* @param count: The number of elements in the array.
* @param tag: The memory tag used for tracking deallocations. Defaults to `UNKNOWN`.
*/
CUSTOM_DELETE_ARRAY(NOUS_DELETE_ARRAY)