#pragma once

#include <string>

namespace NOUS_FileManager 
{
	// Path utilities
	
	bool Exists(const std::string& path);
	bool IsDirectory(const std::string& path);

	std::string GetAbsolutePath(const std::string& path);
	std::string GetRelativePath(const std::string& path);
	std::string GetDirectory(const std::string& path);
	std::string GetFilename(const std::string& path);
	std::string GetExtension(const std::string& path);

	// Directory operations
	bool CreateDirectory(const std::string& path);

	// File operations
	bool CopyFile(const std::string& source, const std::string& destination);
	bool MoveFile(const std::string& source, const std::string& destination);
	bool DeleteFile(const std::string& path);
}