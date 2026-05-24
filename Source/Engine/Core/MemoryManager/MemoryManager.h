#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <string>
#include <utility>
#include <array>

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

enum class MemoryTag : uint8_t
{
	UNKNOWN = 0,

	THREAD,
	ARRAY,
	DICT,
	APPLICATION,
	RENDERER,
	SCENE,
	GAMEOBJECT,
	SCRIPTING_SYSTEM,
	CAMERA,
	COMPONENT,
	INPUT,
	LINEAR_ALLOCATOR,
	FILE,
	RESOURCE_MESH,
	RESOURCE_TEXTURE,
	RESOURCE_MATERIAL,
	RESOURCE_SHADER,
	EDITOR,
	AUDIO,

	MAX
};

namespace nous::engine::memory
{
    NOUS_ENGINE_API void InitializeMemory(uint64 preAllocatedMemorySize);

    NOUS_ENGINE_API void ShutdownMemory();

    NOUS_ENGINE_API void* Allocate(uint64 size, MemoryTag tag);

    NOUS_ENGINE_API void Free(void* block, MemoryTag tag);
    NOUS_ENGINE_API void Free(void* block, uint64 size, MemoryTag tag);

    NOUS_ENGINE_API void* ZeroMemory(void* block, uint64 size);

    NOUS_ENGINE_API void* CopyMemory(void* destination, const void* source, uint64 size);

    NOUS_ENGINE_API void* SetMemory(void* destination, int32 value, uint64 size);

    NOUS_ENGINE_API std::string GetMemoryUsageStats();

    NOUS_ENGINE_API uint64 GetMemoryAllocationCount();

	struct MemoryStatsSnapshot
	{
		uint64 totalAllocated;
		uint64 totalAllocations;
		std::array<uint64, static_cast<uint64>(std::to_underlying(MemoryTag::MAX))> taggedAllocations;
	};

	NOUS_ENGINE_API MemoryStatsSnapshot GetMemoryStats();
	NOUS_ENGINE_API const char* const* GetMemoryTagNames();

	struct MemoryConfigSnapshot
	{
		uint64 totalAllocationSize;
		uint64 allocatorRequirement;
	};

	NOUS_ENGINE_API MemoryConfigSnapshot GetMemoryConfig();
}

// Custom Memory Management Macros to monitorize allocations

#define CUSTOM_NEW(name) \
template<typename T, typename... Args> \
T* name(MemoryTag tag = MemoryTag::UNKNOWN, Args&&... args) \
{ \
    void* memory = nous::engine::memory::Allocate(sizeof(T), tag); \
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
T* name(size_t count, MemoryTag tag = MemoryTag::UNKNOWN) \
{ \
    void* memory = nous::engine::memory::Allocate(sizeof(T) * count, tag); \
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
inline void name(T*& ptr, MemoryTag tag = MemoryTag::UNKNOWN) noexcept \
{ \
    if (ptr != nullptr) \
    { \
        ptr->~T(); \
        nous::engine::memory::Free(static_cast<void*>(ptr), tag); \
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
void name(T*& ptr, size_t count, MemoryTag tag = MemoryTag::UNKNOWN) \
{ \
    if (ptr != nullptr) \
    { \
        for (size_t i = 0; i < count; ++i) \
        { \
            ptr[i].~T(); \
        } \
        nous::engine::memory::Free(ptr, sizeof(T) * count, tag); \
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

#endif // MEMORYMANAGER_H