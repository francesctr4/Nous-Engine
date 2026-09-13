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

	// Moves the read cursor to an absolute byte offset.
	//
	// Required by any binary format carrying an offset directory: without it such a
	// format has to be read from the front, which is exactly what the directory
	// exists to avoid. The mesh library binary (V4) is the first user -- it reads
	// one submesh out of N without touching the other N-1.
	//
	// Clears EOF before seeking, so a handle that already hit the end stays usable.
	// Returns false if the file is not open for reading or the seek fails.
	bool Seek(uint64_t offset);

	// Total size in bytes. Cheap: seeks to the end and restores the cursor.
	std::optional<uint64_t> Size();

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