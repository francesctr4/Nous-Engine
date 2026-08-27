#include <FileWatcher/FileWatcher.h>

#include <Logger/Logger.h>

void FileWatcher::Watch(const std::string& path, Callback callback)
{
    std::filesystem::file_time_type lastWrite{};

    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        lastWrite = std::filesystem::last_write_time(path, ec);

    m_entries[path] = Entry{ std::move(callback), lastWrite };
}

void FileWatcher::WatchIfNew(const std::string& path, Callback callback)
{
    if (m_entries.contains(path)) return;
    Watch(path, std::move(callback));
}

bool FileWatcher::IsWatched(const std::string& path) const
{
    return m_entries.contains(path);
}

void FileWatcher::Unwatch(const std::string& path)
{
    m_entries.erase(path);
}

void FileWatcher::Clear()
{
    m_entries.clear();
}

void FileWatcher::Poll()
{
    for (auto& [path, entry] : m_entries)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            continue;

        auto currentWrite = std::filesystem::last_write_time(path, ec);
        if (ec)
            continue;

        if (currentWrite != entry.lastWriteTime)
        {
            entry.lastWriteTime = currentWrite;
            entry.callback(path);
        }
    }
}
