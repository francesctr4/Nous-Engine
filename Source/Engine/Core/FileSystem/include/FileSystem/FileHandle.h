#ifndef FILEHANDLE_H
#define FILEHANDLE_H

#include <string>
#include <memory>
#include <fstream>
#include <cstdint>

enum class FileMode : int8_t
{
	UNKNOWN = -1,

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

	bool ReadBytes(uint64_t dataSize, char* outReadData, uint64_t* outBytesRead);
	bool ReadAllBytes(char** outBytes, uint64_t* outBytesRead);
	
	bool ReadLine(std::string& outLine);
	bool WriteLine(const std::string& line);
	 
	bool Write(uint64_t dataSize, const void* data, uint64_t* outBytesWritten);

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