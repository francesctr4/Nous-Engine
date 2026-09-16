#pragma once

#include <filesystem>
#include <string>

#include <EngineCore/EngineExport.h>

namespace nous::engine::filesystem
{
	// Path utilities
	
	NOUS_ENGINE_API bool Exists(const std::string& path);
	NOUS_ENGINE_API bool IsDirectory(const std::string& path);

	NOUS_ENGINE_API std::string GetAbsolutePath(const std::string& path);
	NOUS_ENGINE_API std::string GetRelativePath(const std::string& path);
	NOUS_ENGINE_API std::string GetDirectory(const std::string& path);
	NOUS_ENGINE_API std::string GetFilename(const std::string& path);
	NOUS_ENGINE_API std::string GetExtension(const std::string& path);

	// Returns the path with all backslashes replaced by forward slashes.
	NOUS_ENGINE_API std::string NormalizePath(const std::string& path);

	// The counterpart to NormalizePath: returns the path spelled the way the OS
	// spells it, for handing to an OS API that parses the string literally.
	// Engine-internal paths stay normalized; convert only at the handoff.
	//
	// On POSIX this is the identity, deliberately — a backslash there is a legal
	// filename character, so rewriting one names a different file.
	NOUS_ENGINE_API std::string ToNativePath(const std::string& path);

	// Directory operations
	NOUS_ENGINE_API bool CreateDirectory(const std::filesystem::path& path);
	NOUS_ENGINE_API bool DeleteDirectory(const std::filesystem::path& path);

	// File operations
	NOUS_ENGINE_API bool CopyFile(const std::string& source, const std::string& destination);
	NOUS_ENGINE_API bool MoveFile(const std::string& source, const std::string& destination);
	NOUS_ENGINE_API bool DeleteFile(const std::string& path);

	// Shell integration

	// Opens the OS file manager at `path`, accepting an engine-relative path. A
	// file is revealed with the file itself selected; a directory is opened.
	//
	// Returns false when the path does not exist or the file manager could not be
	// launched. On POSIX a true means only that the helper was spawned — whether
	// xdg-open/open then honoured it is not reported back.
	NOUS_ENGINE_API bool RevealInFileManager(const std::string& path);
}