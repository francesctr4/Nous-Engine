#ifndef FILEHANDLE_H
#define FILEHANDLE_H

#include <Utils/DataStructures/NOUS_Vector.h>

#include <string>
#include <memory>
#include <fstream>
#include <cstdint>
#include <optional>
#include <span>

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

	std::optional<uint64_t> ReadBytes(std::span<char> outBuffer);
	std::optional<NOUS_Vector<char>> ReadAllBytes();

	bool ReadLine(std::string& outLine);
	bool WriteLine(const std::string& line);

	std::optional<uint64_t> Write(std::span<const std::byte> data);

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