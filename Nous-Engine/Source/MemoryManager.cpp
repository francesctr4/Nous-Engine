#include "MemoryManager.h"

#include "Logger.h"
#include "Asserts.h"
#include "DynamicAllocator.h"

#ifdef _PROFILING
#include "Tracy.h"
#endif // _PROFILING

#include <mutex>

static std::mutex memoryMutex;

struct MemoryStats 
{
	uint64 totalAllocated;
	uint64 totalAllocations;
	uint64 taggedAllocations[static_cast<uint64>(MemoryManager::MemoryTag::MAX)];
};

struct MemorySystemConfig 
{
	MemoryStats stats;

	uint64 totalAllocationSize;
	uint64 allocatorRequirement;

	DynamicAllocator* allocator;
	void* allocatorBlock;
};

static const char* memoryTagStrings[static_cast<uint64>(MemoryManager::MemoryTag::MAX)] = 
{
	"UNKNOWN			",
	"THREAD			",
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
	"SCENE			",
	"INPUT			",
	"LINEAR_ALLOC		",
	"FILE			",
	"RESOURCE_MESH		",
	"RESOURCE_TEXTURE	",
	"RESOURCE_MATERIAL	"
};

static struct MemorySystemConfig config;

void MemoryManager::InitializeMemory()
{
	std::memset(&config, 0, sizeof(config));

	config.totalAllocationSize = GIBIBYTES(1);

	// 1. Get memory requirement FIRST
	config.allocatorRequirement = DynamicAllocator::GetMemoryRequirement(config.totalAllocationSize);

	// 2. Allocate single block for entire system
	config.allocatorBlock = malloc(config.allocatorRequirement);
	if (!config.allocatorBlock) {
		NOUS_FATAL("Memory system allocation failed");
		return;
	}

	// 3. Construct allocator using placement new
	config.allocator = new DynamicAllocator(
		config.totalAllocationSize,
		config.allocatorBlock
	);

	NOUS_DEBUG("Memory system initialized with %llu bytes", config.totalAllocationSize);
}

void MemoryManager::ShutdownMemory()
{
	if (config.allocator) {
		// Explicit destructor call
		config.allocator->~DynamicAllocator();
		free(config.allocatorBlock);
		config.allocatorBlock = nullptr;
		config.allocator = nullptr;
	}

	// ----------------------------------------------------------------------- //

	MemoryTag tag = MemoryTag::UNKNOWN;
	float amount = 0.0f;

	for (uint32 i = 0; i < static_cast<uint64>(MemoryTag::MAX); ++i)
	{
		tag = static_cast<MemoryTag>(i);
		amount = static_cast<float>(config.stats.taggedAllocations[i]);
		
		NOUS_ASSERT_MSG(amount == 0.0f, "Memory Leaks Detected!");
	}

	NOUS_ASSERT(config.stats.totalAllocations == 0);
	NOUS_ASSERT(config.stats.totalAllocated == 0);

	// ----------------------------------------------------------------------- //
}

void* MemoryManager::Allocate(uint64 size, MemoryTag tag = MemoryTag::UNKNOWN)
{
	std::lock_guard<std::mutex> lock(memoryMutex);

	if (tag == MemoryTag::UNKNOWN) 
	{
		NOUS_WARN("Memory Allocation called using MEMORY_TAG_UNKNOWN.");
	}

	config.stats.totalAllocated += size;
	config.stats.totalAllocations++;
	config.stats.taggedAllocations[static_cast<uint64>(tag)] += size;

	// TODO: Memory Alignment
	//void* block = malloc(size);
		// Add 16-byte alignment
	const uint64 alignment = 16;
	const uint64 aligned_size = (size + (alignment - 1)) & ~(alignment - 1);

	void* block = config.allocator->Allocate(aligned_size);
	ZeroMemory(block, size);

#ifdef _PROFILING
	TracyAlloc(block, size);
#endif // _PROFILING

	return block;
}

void MemoryManager::Free(void* block, uint64 size, MemoryTag tag = MemoryTag::UNKNOWN)
{
	std::lock_guard<std::mutex> lock(memoryMutex);

	if (tag == MemoryTag::UNKNOWN)
	{
		NOUS_WARN("Memory Free called using MEMORY_TAG_UNKNOWN.");
	}

	config.stats.totalAllocated -= size;
	config.stats.totalAllocations--;
	config.stats.taggedAllocations[static_cast<uint64>(tag)] -= size;

#ifdef _PROFILING
	TracyFree(block);
#endif // _PROFILING

	// TODO: Memory Alignment
	//free(block);
	config.allocator->Free(block, size);
}

void* MemoryManager::ZeroMemory(void* block, uint64 size)
{
	return std::memset(block, 0, size);
}

void* MemoryManager::CopyMemory(void* destination, const void* source, uint64 size)
{
	return std::memcpy(destination, source, size);
}

void* MemoryManager::SetMemory(void* destination, int32 value, uint64 size)
{
	return std::memset(destination, value, size);
}

char* MemoryManager::GetMemoryUsageStats()
{
	const uint64 GiB = 1024 * 1024 * 1024;
	const uint64 MiB = 1024 * 1024;
	const uint64 KiB = 1024;

	char buffer[8000] = "System memory use (tagged):\n";
	uint64 offset = strlen(buffer);

	// Log total size allocated in appropriate units
	char totalUnit[4] = "XiB";
	float totalAmount = 1.0f;

	if (config.stats.totalAllocated >= GiB)
	{
		totalUnit[0] = 'G';
		totalAmount = config.stats.totalAllocated / static_cast<float>(GiB);
	}
	else if (config.stats.totalAllocated >= MiB)
	{
		totalUnit[0] = 'M';
		totalAmount = config.stats.totalAllocated / static_cast<float>(MiB);
	}
	else if (config.stats.totalAllocated >= KiB)
	{
		totalUnit[0] = 'K';
		totalAmount = config.stats.totalAllocated / static_cast<float>(KiB);
	}
	else
	{
		totalUnit[0] = 'B';
		totalUnit[1] = 0;
		totalAmount = static_cast<float>(config.stats.totalAllocated);
	}

	snprintf(buffer + offset, 8000 - offset, "\nTotal size allocated: %.2f %s\n", totalAmount, totalUnit);
	offset += strlen(buffer + offset);

	// Log total allocations
	snprintf(buffer + offset, 8000 - offset, "Total allocations: %llu\n\n", config.stats.totalAllocations);
	offset += strlen(buffer + offset);

	// Log allocations by tag
	for (uint32 i = 0; i < static_cast<uint64>(MemoryTag::MAX); ++i)
	{
		char unit[4] = "XiB";
		float amount = 1.0f;

		if (config.stats.taggedAllocations[i] >= GiB)
		{
			unit[0] = 'G';
			amount = config.stats.taggedAllocations[i] / static_cast<float>(GiB);
		}
		else if (config.stats.taggedAllocations[i] >= MiB)
		{
			unit[0] = 'M';
			amount = config.stats.taggedAllocations[i] / static_cast<float>(MiB);
		}
		else if (config.stats.taggedAllocations[i] >= KiB)
		{
			unit[0] = 'K';
			amount = config.stats.taggedAllocations[i] / static_cast<float>(KiB);
		}
		else 
		{
			unit[0] = 'B';
			unit[1] = 0;
			amount = static_cast<float>(config.stats.taggedAllocations[i]);
		}

		int32 length = snprintf(buffer + offset, 8000, "%s: %.2f %s\n", memoryTagStrings[i], amount, unit);
		offset += length;
 	}

	return _strdup(buffer);
}

uint64 MemoryManager::GetMemoryAllocationCount()
{
	return config.stats.totalAllocations;
}
