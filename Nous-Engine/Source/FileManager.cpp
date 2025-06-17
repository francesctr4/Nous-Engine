#include "FileManager.h"

#include "Logger.h"

// Wrapper around std::filesystem
#include <filesystem>

std::string NOUS_FileManager::GetAbsolutePath(const std::string& path)
{
	try
	{
		return std::filesystem::absolute(path).string();
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		NOUS_ERROR("File system error getting absolute path: %s", e.what());
		return {};
	}
}

std::string NOUS_FileManager::GetRelativePath(const std::string& path)
{
	try
	{
		return std::filesystem::relative(path).string();
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		NOUS_ERROR("File system error getting relative path: %s", e.what());
		return {};
	}
}

std::string NOUS_FileManager::GetDirectory(const std::string& path)
{
	return std::filesystem::relative(path).parent_path().string() + "\\";
}

bool NOUS_FileManager::Exists(const std::string& path)
{
	return std::filesystem::exists(path);
}

bool NOUS_FileManager::IsDirectory(const std::string& path)
{
	return std::filesystem::is_directory(path);
}

std::string NOUS_FileManager::GetFilename(const std::string& path)
{
	return std::filesystem::path(path).stem().string();
}

std::string NOUS_FileManager::GetExtension(const std::string& path)
{
	return std::filesystem::path(path).extension().string();
}

bool NOUS_FileManager::CreateDirectory(const std::string& path)
{
	if (!Exists(path)) 
	{
		try
		{
			return std::filesystem::create_directories(path);
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			NOUS_ERROR("File system error creating directory: %s", e.what());
			return false;
		}
	}

	return true;
}

bool NOUS_FileManager::DeleteDirectory(const std::string& path)
{
	if (Exists(path))
	{
		// Try to remove the directory
		try
		{
			// Iterate over the directory and remove its contents
			for (const auto& entry : std::filesystem::directory_iterator(path))
			{
				if (std::filesystem::is_directory(entry))
				{
					// Recursively remove subdirectories
					DeleteDirectory(entry.path().string());
				}
				else
				{
					// Remove files
					std::filesystem::remove(entry);
				}
			}

			// Remove the empty directory after cleaning it up
			std::filesystem::remove(path);
		}
		catch (...)
		{
			// Handle any errors that occur during the process
			return false;
		}
	}

	return true;
}

bool NOUS_FileManager::CopyFile(const std::string& source, const std::string& destination)
{
	try
	{
		// Check if the destination is a directory
		std::filesystem::path destinationPath(destination);

		if (std::filesystem::is_directory(destinationPath))
		{
			// Append the source file name to the destination directory path
			std::filesystem::path sourcePath(source);
			destinationPath /= sourcePath.filename();
		}

		// Perform the file copy operation
		return std::filesystem::copy_file(source, destinationPath, std::filesystem::copy_options::overwrite_existing);
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		NOUS_ERROR("File system error copying file: %s", e.what());
		return false;
	}
}

bool NOUS_FileManager::MoveFile(const std::string& source, const std::string& destination)
{
	try
	{
		std::filesystem::rename(source, destination);
		return true;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		NOUS_ERROR("File system error moving file: %s", e.what());
		return false;
	}
}

bool NOUS_FileManager::DeleteFile(const std::string& path)
{
	try
	{
		return std::filesystem::remove(path);
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		NOUS_ERROR("File system error deleting file: %s", e.what());
		return false;
	}
}