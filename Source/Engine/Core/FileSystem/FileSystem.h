#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <filesystem>
#include <string>

#include "Engine/EngineExport.h"

namespace NOUS_FileManager 
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

	// Directory operations
	NOUS_ENGINE_API bool CreateDirectory(const std::filesystem::path& path);
	NOUS_ENGINE_API bool DeleteDirectory(const std::filesystem::path& path);

	// File operations
	NOUS_ENGINE_API bool CopyFile(const std::string& source, const std::string& destination);
	NOUS_ENGINE_API bool MoveFile(const std::string& source, const std::string& destination);
	NOUS_ENGINE_API bool DeleteFile(const std::string& path);
}

#endif // FILEMANAGER_H