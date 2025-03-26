#include "ImporterTexture.h"
#include "RendererFrontend.h"
#include "FileHandle.h"
#include "FileManager.h"

#include "ModuleRenderer3D.h"
#include "MetaFileData.inl"

#include "ResourceTexture.h"
#include "MemoryManager.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_THREAD_LOCAL
#include "External/stb_image/stb_image.h"

bool ImporterTexture::Import(const MetaFileData& metaFileData)
{
    Resource* tempTexture = NOUS_NEW<ResourceTexture>(MemoryManager::MemoryTag::RESOURCE_TEXTURE);
    return Save(metaFileData, tempTexture);
}

bool ImporterTexture::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    ResourceTexture* texture = static_cast<ResourceTexture*>(inResource);
    if (!texture) return false;
    NOUS_DELETE<ResourceTexture>(texture, MemoryManager::MemoryTag::RESOURCE_TEXTURE);

    bool ret = NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    return ret;
}

bool ImporterTexture::Load(const std::string& libraryPath, Resource* outResource)
{
    ResourceTexture* texture = static_cast<ResourceTexture*>(outResource);
    if (!texture) return false;

    const int32 requiredChannelCount = 4;

    stbi_set_flip_vertically_on_load_thread(true);

    // Use FileHandle to read the file
    FileHandle fileHandle;
    if (!fileHandle.Open(libraryPath, FileMode::READ, true)) // true for binary mode
    {
        NOUS_WARN("ImporterTexture::Load() failed to open file '%s'", libraryPath.c_str());
        return false;
    }

    char* fileBuffer = nullptr;
    uint64 fileSize = 0;

    // Read all bytes using your class's method
    if (!fileHandle.ReadAllBytes(&fileBuffer, &fileSize) || !fileBuffer || fileSize == 0)
    {
        NOUS_WARN("ImporterTexture::Load() failed to read file '%s'", libraryPath.c_str());
        fileHandle.Close();
        return false;
    }

    // Load from memory buffer
    int width, height, channels;
    uint8* data = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(fileBuffer), // Cast to stbi's expected type
        static_cast<int>(fileSize),
        &width,
        &height,
        &channels,
        requiredChannelCount
    );

    // Clean up file buffer using your memory manager
    NOUS_DELETE_ARRAY(fileBuffer, static_cast<uint64>(fileSize), MemoryManager::MemoryTag::FILE);
    fileHandle.Close();

    if (data)
    {
        texture->width = width;
        texture->height = height;
        texture->channelCount = requiredChannelCount;

        const uint64 totalSize = texture->width * texture->height * requiredChannelCount;

        // Check for transparency
        bool hasTransparency = false;
        for (uint64 i = 0; i < totalSize; i += requiredChannelCount)
        {
            if (data[i + 3] < 255)
            {
                hasTransparency = true;
                break;
            }
        }

        uint32 currentGeneration = texture->GetReferenceCount() == 0 ? INVALID_ID : texture->GetReferenceCount();
        texture->generation = (currentGeneration == INVALID_ID) ? 0 : currentGeneration;

        // Acquire internal texture resources and upload to GPU.
        External->renderer->rendererFrontend->CreateTexture(data, texture);

        // Clean up data.
        stbi_image_free(data);

        return true;
    }
    else
    {
        if (stbi_failure_reason())
        {
            NOUS_WARN("ImporterTexture::Load() failed to load file '%s': %s", 
                libraryPath.c_str(), stbi_failure_reason());
        }

        return false;
    }
}

bool ImporterTexture::Unload(Resource* inResource)
{
    ResourceTexture* texture = static_cast<ResourceTexture*>(inResource);
    if (!texture) return false;

    External->renderer->rendererFrontend->DestroyTexture(texture);

    return true;
}
