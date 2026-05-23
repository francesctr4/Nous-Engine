#ifndef FILEHANDLE_H
#define FILEHANDLE_H

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

#include <string>
#include <memory>
#include <fstream>

enum class FileMode : int8_t
{
	UNKNOWN = -1,

	READ,
	WRITE,
	READ_AND_WRITE
};

class NOUS_ENGINE_API FileHandle
{
public:

	FileHandle();
	~FileHandle();

	bool Open(const std::string& filePath, FileMode mode, bool isBinary);
	void Close();

	bool ReadBytes(uint64 dataSize, char* outReadData, uint64* outBytesRead);
	bool ReadAllBytes(char** outBytes, uint64* outBytesRead);
	
	bool ReadLine(std::string& outLine);
	bool WriteLine(const std::string& line);
	 
	bool Write(uint64 dataSize, const void* data, uint64* outBytesWritten);

	// ---------------------------------------------------------------------------- //

	void SetPath(const std::string& filePath);
	std::string GetPath();
	
	bool IsOpen();

private:

	std::unique_ptr<std::fstream> fileStream;
	std::string path;

	FileMode mode;

};

#endif // FILEHANDLE_H