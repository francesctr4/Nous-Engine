#pragma once

#include "Globals.h"

#include <memory>
#include <fstream>

enum class FileMode
{
	READ,
	WRITE,
	READ_AND_WRITE
};

class FileHandle
{
public:

	FileHandle();
	~FileHandle();

	bool Open(const std::string& filePath, FileMode mode, bool isBinary);
	void Close();

	bool ReadBytes(uint64 dataSize, char* outReadData, uint64* outBytesRead);
	bool ReadAllBytes(char** outBytes, uint64* outBytesRead);
	
	//bool ReadLine(std::string& outLine);
	//bool WriteLine(std::string line);
	 
	//bool Write(uint64 dataSize, const void* data, uint64* outBytesWritten);

	// ---------------------------------------------------------------------------- //

	void SetPath(const std::string& filePath);
	std::string GetPath();
	
	bool IsOpen();

private:

	std::unique_ptr<std::fstream> fileStream;
	std::string path;

};