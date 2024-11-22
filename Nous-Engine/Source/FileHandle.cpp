#include "FileHandle.h"

#include "MemoryManager.h"

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

bool FileHandle::Open(const std::string& filePath, FileMode mode, bool isBinary)
{
    if (IsOpen())
    {
        Close();
    }

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

bool FileHandle::ReadBytes(uint64 dataSize, char* outReadData, uint64* outBytesRead)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read bytes from a closed or invalid file.");
        return false;
    }

    // Ensure the file is open in read mode
    if (mode != FileMode::READ && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in read mode.");
        return false;
    }

    // Check if the output buffer is valid
    if (!outReadData || !outBytesRead)
    {
        NOUS_ERROR("Invalid output buffer or bytes read pointer.");
        return false;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for reading.");
        return false;
    }

    // Attempt to read the specified number of bytes
    fileStream->read(outReadData, dataSize);

    // Check how many bytes were actually read
    *outBytesRead = static_cast<uint64>(fileStream->gcount());

    // Log an error if fewer bytes were read than requested
    if (*outBytesRead < dataSize)
    {
        NOUS_WARN("Only %llu out of %llu bytes were read from the file.", *outBytesRead, dataSize);
    }

    // Return true if at least some bytes were successfully read
    return (*outBytesRead > 0);
}

bool FileHandle::ReadAllBytes(char** outBytes, uint64* outBytesRead)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read all bytes from a closed or invalid file.");
        return false;
    }

    // Ensure the file is open in read mode
    if (mode != FileMode::READ && mode != FileMode::READ_AND_WRITE)
    {
        NOUS_ERROR("File is not opened in read mode.");
        return false;
    }

    // Check if the output pointers are valid
    if (!outBytes || !outBytesRead)
    {
        NOUS_ERROR("Invalid output pointers provided.");
        return false;
    }

    // Move to the end of the file to determine its size
    fileStream->seekg(0, std::ios::end);
    std::streampos fileSize = fileStream->tellg();

    if (fileSize <= 0)
    {
        NOUS_ERROR("Failed to determine the file size or the file is empty.");
        return false;
    }

    // Allocate memory for the file content
    *outBytes = NOUS_NEW_ARRAY<char>(static_cast<uint64>(fileSize), MemoryManager::MemoryTag::FILE);

    if (!(*outBytes))
    {
        NOUS_ERROR("Failed to allocate memory for reading file contents.");
        return false;
    }

    // Read the file content
    fileStream->seekg(0, std::ios::beg);
    fileStream->read(*outBytes, fileSize);

    // Check how many bytes were actually read
    *outBytesRead = static_cast<uint64>(fileStream->gcount());

    // Validate if the entire file was read
    if (*outBytesRead != static_cast<uint64>(fileSize))
    {
        NOUS_WARN("Only %llu out of %llu bytes were read from the file.", *outBytesRead, static_cast<uint64>(fileSize));
    }

    // Return true if at least some bytes were successfully read
    return (*outBytesRead > 0);
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

bool FileHandle::WriteLine(std::string line)
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

bool FileHandle::Write(uint64 dataSize, const void* data, uint64* outBytesWritten)
{
    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to write data to a closed or invalid file.");
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
        NOUS_ERROR("File stream is not in a good state for writing data.");
        return false;
    }

    // Write the data to the file
    fileStream->write(static_cast<const char*>(data), dataSize);

    // Check how many bytes were actually written
    if (outBytesWritten)
    {
        *outBytesWritten = static_cast<uint64>(fileStream->tellp());
    }

    // Check for write failures
    if (fileStream->fail())
    {
        NOUS_ERROR("Failed to write data to the file.");
        return false;
    }

    return true;
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