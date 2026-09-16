// Its own translation unit, not part of FileSystem.cpp, because <windows.h>
// #defines CopyFile, MoveFile, DeleteFile and CreateDirectory to their A/W
// variants -- the exact four names this namespace already exports. Pulling it
// into FileSystem.cpp would rename those definitions out from under their
// declarations, so the platform headers are quarantined here instead.

#include <FileSystem/FileSystem.h>

#include <Logger/Logger.h>

#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
#ifndef _WIN32
	// Spawns a detached helper. The double fork is what keeps it detached without
	// leaking a zombie: the intermediate child exits immediately so the parent's
	// waitpid returns at once, and the grandchild is reparented to init.
	bool SpawnDetached(const char* program, const char* const argv[])
	{
		const pid_t intermediate = fork();
		if (intermediate < 0)
		{
			NOUS_ERROR("Failed to fork for '%s'", program);
			return false;
		}

		if (intermediate == 0)
		{
			if (fork() == 0)
			{
				execvp(program, const_cast<char* const*>(argv));
				// Only reachable when exec failed. _exit, never return: returning
				// would resume the parent's code inside a forked copy.
				_exit(127);
			}
			_exit(0);
		}

		int status = 0;
		waitpid(intermediate, &status, 0);
		return true;
	}
#endif
}

bool nous::engine::filesystem::RevealInFileManager(const std::string& path)
{
	if (!Exists(path))
	{
		NOUS_WARN("Cannot reveal '%s': the path does not exist", path.c_str());
		return false;
	}

	const std::string absolute = GetAbsolutePath(path);
	if (absolute.empty())
		return false;

	const bool isDirectory = IsDirectory(absolute);

#ifdef _WIN32
	// Explorer parses this argument literally, so the separators must be native:
	// given forward slashes it silently opens Documents instead of failing.
	const std::string native = ToNativePath(absolute);
	const std::string parameters = isDirectory
		                               ? "\"" + native + "\""
		                               : "/select,\"" + native + "\"";

	const HINSTANCE result = ShellExecuteA(nullptr, "open", "explorer.exe",
	                                       parameters.c_str(), nullptr, SW_SHOWNORMAL);

	// ShellExecute reports failure through the return value's numeric range --
	// anything <= 32 is an error code wearing an HINSTANCE's type.
	if (reinterpret_cast<INT_PTR>(result) <= 32)
	{
		NOUS_ERROR("Failed to reveal '%s' in Explorer (code %d)",
		           absolute.c_str(), static_cast<int>(reinterpret_cast<INT_PTR>(result)));
		return false;
	}

	return true;
#elif defined(__APPLE__)
	if (isDirectory)
	{
		const char* const argv[] = {"open", absolute.c_str(), nullptr};
		return SpawnDetached("open", argv);
	}

	const char* const argv[] = {"open", "-R", absolute.c_str(), nullptr};
	return SpawnDetached("open", argv);
#else
	// No portable way to select a file in a Linux file manager, so a file is
	// revealed by opening the directory that holds it.
	const std::string target = isDirectory
		                           ? absolute
		                           : std::filesystem::path(absolute).parent_path().string();

	const char* const argv[] = {"xdg-open", target.c_str(), nullptr};
	return SpawnDetached("xdg-open", argv);
#endif
}
