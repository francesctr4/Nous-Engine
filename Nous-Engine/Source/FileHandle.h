#pragma once

#include <memory>
#include <fstream>
#include <string>

enum class FileMode
{
	READ,
	WRITE,
	READ_AND_WRITE
};

class FileHandle
{
public:

	// Open the file
	bool Open(const std::string& filePath, FileMode mode);

	// Getters
	std::string GetPath();
	void SetPath();
	bool IsValid(); // Shouldnt be IsOpen()?

private:

	std::unique_ptr<std::fstream> fileStream;

	std::string path;
	bool isValid;
};