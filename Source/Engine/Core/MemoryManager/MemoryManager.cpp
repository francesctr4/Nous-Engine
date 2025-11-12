#include "MemoryManager.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/MemoryManager/CustomAllocators/DynamicAllocator/include/DynamicAllocator.h"

#include <magic_enum/magic_enum.hpp>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif // _PROFILING

#include <mutex>
#include <cstring>
#include <sstream>
#include <iomanip>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_MEMORYMANAGER;

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
	"RESOURCE_MATERIAL	",
    "EDITOR	                "
};

static struct MemorySystemConfig config;

void MemoryManager::InitializeMemory(uint64 preAllocatedMemorySize)
{
	ZeroMemory(&config, sizeof(config));

	config.totalAllocationSize = preAllocatedMemorySize;

	// 1. Get memory requirement FIRST
	config.allocatorRequirement = DynamicAllocator::GetMemoryRequirement(config.totalAllocationSize);

	// 2. Allocate single block for entire system
	config.allocatorBlock = malloc(config.allocatorRequirement);

	if (!config.allocatorBlock) 
	{
		NOUS_FATAL_C(CURRENT_CHANNEL, "[%s] Memory system allocation failed", __FUNCTION__);
		return;
	}

	// 3. Construct allocator using placement new
	config.allocator = new (&config.allocatorBlock) DynamicAllocator(
			config.totalAllocationSize,
			config.allocatorBlock
	);

	NOUS_DEBUG_C(CURRENT_CHANNEL, "[%s] Memory system initialized with %llu bytes",
			   __FUNCTION__, config.totalAllocationSize);
}

void MemoryManager::ShutdownMemory()
{
	if (config.allocator->GetFreeSpace() != config.totalAllocationSize)
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "Possible leaks detected: not all memory returned to allocator!");
	}

	if (config.allocator) 
	{
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
		NOUS_WARN_C(CURRENT_CHANNEL, "[%s] Memory Allocation called using MEMORY_TAG_UNKNOWN.", __FUNCTION__);
	}

	config.stats.totalAllocated += size;
	config.stats.totalAllocations++;
	config.stats.taggedAllocations[static_cast<uint64>(tag)] += size;

	void* block = config.allocator->Allocate(size); // allocator aligns internally
	ZeroMemory(block, size);                         // zero raw bytes (fine)

#ifdef _PROFILING
	TracyAlloc(block, size);
#endif // _PROFILING

	return block;
}

void MemoryManager::Free(void* block, MemoryTag tag)
{
	if (!block) return;
	std::lock_guard<std::mutex> lock(memoryMutex);

#ifdef _PROFILING
	TracyFree(block);
#endif

	const uint64 raw = config.allocator->GetRecordedSize(block);
	if (raw == 0)
	{
		// Not tracked by allocator: do not touch stats to avoid underflow
		NOUS_WARN_C(CURRENT_CHANNEL,
					"Free(void*, tag=%d): block %p not recorded — skipping stats",
					static_cast<int>(tag), block);

		// Try to free anyway — allocator will warn/return false if unknown
		(void)config.allocator->Free(block);
		return;
	}

	config.stats.totalAllocated -= raw;
	config.stats.totalAllocations--;
	config.stats.taggedAllocations[static_cast<uint64>(tag)] -= raw;

	const bool ok = config.allocator->Free(block);
	if (!ok)
	{
		NOUS_WARN_C(CURRENT_CHANNEL,
					"MemoryManager::Free(void*, tag=%d): allocator failed to free %p",
					static_cast<int>(tag), block);
	}
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

	// Allocator aligns internally
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

std::string MemoryManager::GetMemoryUsageStats()
{
	std::ostringstream out;
	bool anyUsed = false;

	out << "Memory Usage by Tag:\n";

	for (auto [tag, name] : magic_enum::enum_entries<MemoryTag>())
	{
		if (tag == MemoryTag::MAX)
			continue;

		uint64 bytes = config.stats.taggedAllocations[static_cast<uint64>(tag)];
		if (bytes == 0)
			continue;

		anyUsed = true;

		// --------------------------------------------------------
		// Convert units dynamically (B / KiB / MiB / GiB)
		// --------------------------------------------------------
		const char* unit = "B";
		double amount = static_cast<double>(bytes);

		if (bytes >= GiB(1))
		{
			unit = "GiB";
			amount = static_cast<double>(bytes) / static_cast<double>(GiB(1));
		}
		else if (bytes >= MiB(1))
		{
			unit = "MiB";
			amount = static_cast<double>(bytes) / static_cast<double>(MiB(1));
		}
		else if (bytes >= KiB(1))
		{
			unit = "KiB";
			amount = static_cast<double>(bytes) / static_cast<double>(KiB(1));
		}

		// --------------------------------------------------------
		// Print formatted row
		// --------------------------------------------------------
		out << std::setw(24) << std::left << name
			<< ": " << std::fixed << std::setprecision(2)
			<< amount << " " << unit << "\n";
	}

	// --------------------------------------------------------
	// If nothing allocated → print friendly message
	// --------------------------------------------------------
	if (!anyUsed)
	{
		out.str("");
		out.clear();
		out << "[MemoryManager] No memory leaks detected.";
	}

	return out.str();
}

uint64 MemoryManager::GetMemoryAllocationCount()
{
	return config.stats.totalAllocations;
}

MemoryManager::MemoryStatsSnapshot MemoryManager::GetMemoryStats()
{
	std::lock_guard<std::mutex> lock(memoryMutex);
	MemoryStatsSnapshot snapshot{};
	snapshot.totalAllocated = config.stats.totalAllocated;
	snapshot.totalAllocations = config.stats.totalAllocations;

	for (uint64 i = 0; i < static_cast<uint64>(MemoryTag::MAX); ++i)
		snapshot.taggedAllocations[i] = config.stats.taggedAllocations[i];

	return snapshot;
}

const char** MemoryManager::GetMemoryTagNames()
{
	static std::vector<const char*> names;
	names.clear();
	for (auto name : magic_enum::enum_names<MemoryTag>())
		names.push_back(name.data());
	return names.data();
}

MemoryManager::MemoryConfigSnapshot MemoryManager::GetMemoryConfig()
{
	std::lock_guard<std::mutex> lock(memoryMutex);
	MemoryConfigSnapshot snapshot{};
	snapshot.totalAllocationSize = config.totalAllocationSize;
	snapshot.allocatorRequirement = config.allocatorRequirement;
	return snapshot;
}
