#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

#include "Engine/EngineExport.h"

// Polls registered files for modification and fires a callback when a change is detected.
// Designed for editor-side hot reload: call Poll() once per frame.
// Thread-safety: not thread-safe — call from a single thread only.
class NOUS_ENGINE_API FileWatcher
{
public:
    using Callback = std::function<void(const std::string& path)>;

    // Register a file to watch. Fires callback when its last-write-time changes.
    // Calling Watch() on an already-watched path replaces the existing callback
    // AND resets the lastWriteTime baseline — use WatchIfNew() to avoid that.
    void Watch(const std::string& path, Callback callback);

    // Like Watch(), but is a no-op if the path is already registered.
    // Use this for periodic rescans so an in-flight modification isn't silently
    // swallowed by overwriting the baseline mtime.
    void WatchIfNew(const std::string& path, Callback callback);

    // Returns true if the path is currently registered.
    bool IsWatched(const std::string& path) const;

    // Stop watching a file.
    void Unwatch(const std::string& path);

    // Remove all watched files.
    void Clear();

    // Check all watched files for changes and fire callbacks for any that changed.
    // Call once per frame (or at whatever rate is appropriate for hot reload).
    void Poll();

private:
    struct Entry
    {
        Callback callback;
        std::filesystem::file_time_type lastWriteTime;
    };

    std::unordered_map<std::string, Entry> m_entries;
};

#endif // FILEWATCHER_H
