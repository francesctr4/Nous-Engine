#pragma once

#include <ResourceManager/Types/ResourceType.h>

#include <cstdint>

// One resource awaiting a GPU upload, identified by UID rather than by pointer.
//
// Lives here, in Core/, rather than nested inside HotReloader, because it crosses
// the IResourceGpuSync seam: the renderer drains a queue of these every frame and
// must be able to name the type without including Runtime/HotReloader.h (which
// would drag the FileWatcher into every consumer of the interface).
//
// UID rather than ResourceBase*: the producing worker may finish long before the
// renderer drains the queue, and the resource can be evicted in between. The
// renderer resolves the UID through GetLoadedResource and skips a null result.
struct ResourceUpload
{
    uint32_t     uid  = 0;
    ResourceType type = ResourceType::UNKNOWN;
};
