#include <FileSystem/FileHandle.h>

#include <FileSystem/FileSystem.h>
#include <MemoryManager/MemoryManager.h>
#include <Logger/Logger.h>

FileHandle::FileHandle() : fileStream(nullptr), mode(FileMode::UNKNOWN)
{

}

FileHandle::~FileHandle()
{
    if (IsOpen()) 
    {
        Close();
    }
}

bool FileHandle::Open(const std::string& rawPath, FileMode mode, bool isBinary)
{
    if (IsOpen())
    {
        Close();
    }

    // Normalize separators so Windows-authored paths (backslashes, e.g. stored in .meta/.nous
    // files) open on POSIX systems. Windows accepts forward slashes too, so this is a no-op there.
    const std::string filePath = nous::engine::filesystem::NormalizePath(rawPath);

    SetPath(filePath);

    this->mode = mode;

    std::ios::openmode openMode = isBinary ? std::ios::binary : std::ios::openmode(0);

    switch (mode) 
    {
        case FileMode::READ: 
        {
            openMode |= std::ios::in;
            break;
        }
        case FileMode::WRITE: 
        {
            openMode |= std::ios::out;
            break;
        }
        case FileMode::READ_AND_WRITE: 
        {
            openMode |= std::ios::in | std::ios::out;
            break;
        }
        default: 
        {
            NOUS_ERROR("Invalid mode passed while trying to open file: '%s'", filePath.c_str());
            return false;
        }
    }

    // Attempt to open the file
    fileStream = std::make_unique<std::fstream>(filePath, openMode);

    if (!fileStream->is_open())
    {
        NOUS_ERROR("Failed to open file: '%s'. Please check the file path and permissions.", filePath.c_str());
        return false;
    }

    return true;
}

void FileHandle::Close()
{
    if (fileStream && fileStream->is_open()) 
    {
        fileStream->close();
    }

    fileStream.reset();
}

std::optional<uint64_t> FileHandle::ReadBytes(std::span<char> outBuffer)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read bytes from a closed or invalid file.");
        return std::nullopt;
    }

    // Ensure the file is open in read mode
    if (mode != FileMode::READ && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in read mode.");
        return std::nullopt;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for reading.");
        return std::nullopt;
    }

    // Attempt to read the specified number of bytes
    fileStream->read(outBuffer.data(), static_cast<std::streamsize>(outBuffer.size()));

    // Check how many bytes were actually read
    const uint64_t bytesRead = static_cast<uint64_t>(fileStream->gcount());

    // Log a warning if fewer bytes were read than requested
    if (bytesRead < outBuffer.size())
    {
        NOUS_WARN("Only %llu out of %llu bytes were read from the file.",
                  bytesRead, static_cast<uint64_t>(outBuffer.size()));
    }

    // Zero bytes is failure, matching the old `return (*outBytesRead > 0);`
    if (bytesRead == 0)
        return std::nullopt;

    return bytesRead;
}

std::optional<NOUS_Vector<char>> FileHandle::ReadAllBytes()
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read all bytes from a closed or invalid file.");
        return std::nullopt;
    }

    // Ensure the file is open in read mode
    if (mode != FileMode::READ && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in read mode.");
        return std::nullopt;
    }

    // Move to the end of the file to determine its size
    fileStream->seekg(0, std::ios::end);
    const std::streampos fileSize = fileStream->tellg();

    if (fileSize <= 0)
    {
        NOUS_ERROR("Failed to determine the file size or the file is empty.");
        return std::nullopt;
    }

    NOUS_Vector<char> buffer(MemoryTag::FILE);
    buffer.resize(static_cast<size_t>(fileSize));

    // Read the file content
    fileStream->seekg(0, std::ios::beg);
    fileStream->read(buffer.data(), fileSize);

    const uint64_t bytesRead = static_cast<uint64_t>(fileStream->gcount());

    // Validate if the entire file was read
    if (bytesRead != static_cast<uint64_t>(fileSize))
    {
        NOUS_WARN("Only %llu out of %llu bytes were read from the file.",
                  bytesRead, static_cast<uint64_t>(fileSize));
    }

    if (bytesRead == 0)
        return std::nullopt;

    // Size down to what was actually read, so length and buffer cannot disagree.
    buffer.resize(static_cast<size_t>(bytesRead));
    return buffer;
}

bool FileHandle::ReadLine(std::string& outLine)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read a line from a closed or invalid file.");
        return false;
    }

    // Ensure the file is open in read mode
    if (mode != FileMode::READ && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in read mode.");
        return false;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for reading a line.");
        return false;
    }

    // Attempt to read a line
    if (std::getline(*fileStream, outLine))
    {
        return true;
    }

    // If reading a line fails, log the error and return false
    NOUS_WARN("Failed to read a line from the file.");

    return false;
}

bool FileHandle::WriteLine(const std::string& line)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to write a line to a closed or invalid file.");
        return false;
    }

    // Ensure the file is open in write mode
    if (mode != FileMode::WRITE && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in write mode.");
        return false;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for writing a line.");
        return false;
    }

    // Write the line and append a newline
    *fileStream << line << '\n';

    // Check for write failures
    if (fileStream->fail())
    {
        NOUS_ERROR("Failed to write a line to the file.");
        return false;
    }

    return true;
}

std::optional<uint64_t> FileHandle::Write(std::span<const std::byte> data)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to write data to a closed or invalid file.");
        return std::nullopt;
    }

    // Ensure the file is open in write mode
    if (mode != FileMode::WRITE && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in write mode.");
        return std::nullopt;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for writing data.");
        return std::nullopt;
    }

    // Write the data to the file
    fileStream->write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    // Check how many bytes were actually written
    const uint64_t bytesWritten = static_cast<uint64_t>(fileStream->tellp());

    // Check for write failures
    if (fileStream->fail())
    {
        NOUS_ERROR("Failed to write data to the file.");
        return std::nullopt;
    }

    return bytesWritten;
}

// --------------------------------------------------------------------------------------------------------------- //

std::string FileHandle::GetPath()
{
    return path;
}

void FileHandle::SetPath(const std::string& filePath)
{
    this->path = filePath;
}

bool FileHandle::IsOpen()
{
    return (fileStream && fileStream->is_open());
}