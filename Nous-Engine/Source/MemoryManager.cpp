#include "MemoryManager.h"

#include "Logger.h"
#include "Asserts.h"

#ifdef _PROFILING
#include "Tracy.h"
#endif // _PROFILING

struct MemoryStats 
{
	uint64 totalAllocated;
	uint64 taggedAllocations[static_cast<uint64>(MemoryManager::MemoryTag::MAX)];
};

static const char* memoryTagStrings[static_cast<uint64>(MemoryManager::MemoryTag::MAX)] = 
{
	"UNKNOWN			",
	"ARRAY			",
	"DARRAY			",
	"DICT			",
	"RING_QUEUE		",
	"BST			",
	"STRING			",
	"APPLICATION		",
	"JOB			",
	"TEXTURE			",
	"MATERIAL_INSTANCE	",
	"RENDERER		",
	"GAME			",
	"TRANSFORM		",
	"ENTITY			",
	"ENTITY_NODE		",
	"SCENE			"
};

static struct MemoryStats stats;

void MemoryManager::InitializeMemory()
{
	ZeroMemory(&stats, sizeof(stats));
}

void MemoryManager::ShutdownMemory()
{
	MemoryTag tag = MemoryTag::UNKNOWN;
	float32 amount = 0.0f;

	for (uint32 i = 0; i < static_cast<uint64>(MemoryTag::MAX); ++i)
	{
		tag = static_cast<MemoryTag>(i);
		amount = static_cast<float32>(stats.taggedAllocations[i]);
		
		NOUS_ASSERT_MSG(amount == 0.0f, "Memory Leaks Detected!");
	}
}

void* MemoryManager::Allocate(uint64 size, MemoryTag tag = MemoryTag::UNKNOWN)
{
	if (tag == MemoryTag::UNKNOWN) 
	{
		NOUS_WARN("Memory Allocation called using MEMORY_TAG_UNKNOWN.");
	}

	stats.totalAllocated += size;
	stats.taggedAllocations[static_cast<uint64>(tag)] += size;

	// TODO: Memory Alignment
	void* block = malloc(size);
	ZeroMemory(block, size);

#ifdef _PROFILING
	TracyAlloc(block, size);
#endif // _PROFILING

	return block;
}

void MemoryManager::Free(void* block, uint64 size, MemoryTag tag = MemoryTag::UNKNOWN)
{
	if (tag == MemoryTag::UNKNOWN)
	{
		NOUS_WARN("Memory Free called using MEMORY_TAG_UNKNOWN.");
	}

	stats.totalAllocated -= size;
	stats.taggedAllocations[static_cast<uint64>(tag)] -= size;

#ifdef _PROFILING
	TracyFree(block);
#endif // _PROFILING

	// TODO: Memory Alignment
	free(block);
}

void* MemoryManager::ZeroMemory(void* block, uint64 size)
{
	return memset(block, 0, size);
}

void* MemoryManager::CopyMemory(void* destination, const void* source, uint64 size)
{
	return memcpy(destination, source, size);
}

void* MemoryManager::SetMemory(void* destination, int32 value, uint64 size)
{
	return memset(destination, value, size);
}

char* MemoryManager::GetMemoryUsageStats()
{
	const uint64 GiB = 1024 * 1024 * 1024;
	const uint64 MiB = 1024 * 1024;
	const uint64 KiB = 1024;

	char buffer[8000] = "System memory use (tagged):\n";
	uint64 offset = strlen(buffer);

	for (uint32 i = 0; i < static_cast<uint64>(MemoryTag::MAX); ++i)
	{
		char unit[4] = "XiB";
		float32 amount = 1.0f;

		if (stats.taggedAllocations[i] >= GiB)
		{
			unit[0] = 'G';
			amount = stats.taggedAllocations[i] / static_cast<float32>(GiB);
		}
		else if (stats.taggedAllocations[i] >= MiB)
		{
			unit[0] = 'M';
			amount = stats.taggedAllocations[i] / static_cast<float32>(MiB);
		}
		else if (stats.taggedAllocations[i] >= KiB)
		{
			unit[0] = 'K';
			amount = stats.taggedAllocations[i] / static_cast<float32>(KiB);
		}
		else 
		{
			unit[0] = 'B';
			unit[1] = 0;
			amount = static_cast<float32>(stats.taggedAllocations[i]);
		}

		int32 length = snprintf(buffer + offset, 8000, "%s: %.2f %s\n", memoryTagStrings[i], amount, unit);
		offset += length;
 	}

	return _strdup(buffer);
}
