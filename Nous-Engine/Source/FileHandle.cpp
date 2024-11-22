#include "FileHandle.h"

#include "MemoryManager.h"

FileHandle::FileHandle() : fileStream(nullptr)
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
    bool ret = false;

    if (IsOpen())
    {
        Close();
    }

    SetPath(filePath);

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
            return ret;
        }
    }

    // Attempt to open the file
    fileStream = std::make_unique<std::fstream>(filePath, openMode);

    ret = fileStream->is_open();

    if (!ret)
    {
        NOUS_ERROR("Failed to open file: '%s'. Please check the file path and permissions.", filePath.c_str());
    }

    return ret;
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
    bool ret = false;

    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read bytes from a closed or invalid file.");
        return ret;
    }

    // Check if the output buffer is valid
    if (!outReadData || !outBytesRead)
    {
        NOUS_ERROR("Invalid output buffer or bytes read pointer.");
        return ret;
    }

    // Ensure the stream is in a good state
    if (!fileStream->good())
    {
        NOUS_ERROR("File stream is not in a good state for reading.");
        return ret;
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
    ret = *outBytesRead > 0;

    return ret;
}

bool FileHandle::ReadAllBytes(char** outBytes, uint64* outBytesRead)
{
    bool ret = false;

    // Check if the file is open and valid
    if (!IsOpen())
    {
        NOUS_ERROR("Attempted to read all bytes from a closed or invalid file.");
        return ret;
    }

    // Check if the output pointers are valid
    if (!outBytes || !outBytesRead)
    {
        NOUS_ERROR("Invalid output pointers provided.");
        return ret;
    }

    // Move to the end of the file to determine its size
    fileStream->seekg(0, std::ios::end);
    std::streampos fileSize = fileStream->tellg();

    if (fileSize <= 0)
    {
        NOUS_ERROR("Failed to determine the file size or the file is empty.");
        return ret;
    }

    // Allocate memory for the file content
    *outBytes = NOUS_NEW_ARRAY<char>(static_cast<uint64>(fileSize), MemoryManager::MemoryTag::FILE);

    if (!(*outBytes))
    {
        NOUS_ERROR("Failed to allocate memory for reading file contents.");
        return ret;
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
    ret = *outBytesRead > 0;

    return ret;
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