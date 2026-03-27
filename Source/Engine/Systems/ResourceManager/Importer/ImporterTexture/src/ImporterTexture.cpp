#include "Engine/Systems/ResourceManager/Importer/ImporterTexture/include/ImporterTexture.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Core/FileSystem/FileHandle/include/FileHandle.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#if defined(_WIN32) || defined(_WIN64)
#define STB_IMAGE_IMPLEMENTATION
#endif
#define STBI_THREAD_LOCAL
#include "stb_image.h"

#include "Engine/Core/Logger/Logger.h"

bool ImporterTexture::Import(const MetaFileData& metaFileData)
{
    Resource* tempTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
    return Save(metaFileData, tempTexture);
}

bool ImporterTexture::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_TEXTURE);

    bool ret = NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    return ret;
}

bool ImporterTexture::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    ResourceTexture* texture = down_cast<ResourceTexture*>(outResource);

    const int32 requiredChannelCount = 4;
    stbi_set_flip_vertically_on_load_thread(true);

    FileHandle fileHandle;
    if (!fileHandle.Open(libraryPath, FileMode::READ, true))
    {
        NOUS_WARN("ImporterTexture::Deserialize() failed to open file '%s'", libraryPath.c_str());
        return false;
    }

    char* fileBuffer = nullptr;
    uint64 fileSize = 0;
    if (!fileHandle.ReadAllBytes(&fileBuffer, &fileSize) || !fileBuffer || fileSize == 0)
    {
        NOUS_WARN("ImporterTexture::Deserialize() failed to read file '%s'", libraryPath.c_str());
        fileHandle.Close();
        return false;
    }

    int width, height, channels;
    uint8* data = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(fileBuffer),
        static_cast<int>(fileSize),
        &width, &height, &channels,
        requiredChannelCount
    );

    NOUS_DELETE_ARRAY(fileBuffer, static_cast<uint64>(fileSize), MemoryTag::FILE);
    fileHandle.Close();

    if (!data)
    {
        if (stbi_failure_reason())
            NOUS_WARN("ImporterTexture::Deserialize() failed for '%s': %s",
                libraryPath.c_str(), stbi_failure_reason());
        return false;
    }

    texture->width        = static_cast<uint32>(width);
    texture->height       = static_cast<uint32>(height);
    texture->channelCount = requiredChannelCount;

    const uint64 totalSize = texture->width * texture->height * requiredChannelCount;

    bool hasTransparency = false;
    for (uint64 i = 0; i < totalSize; i += requiredChannelCount)
    {
        if (data[i + 3] < 255) { hasTransparency = true; break; }
    }
    texture->hasTransparency = hasTransparency;

    uint32 currentGeneration = texture->GetReferenceCount() == 0 ? INVALID_ID : texture->GetReferenceCount();
    texture->generation = (currentGeneration == INVALID_ID) ? 0 : currentGeneration;

    // Copy into the resource's temporary CPU buffer for deferred GPU upload.
    texture->pixelData.assign(data, data + totalSize);
    stbi_image_free(data);

    return true;
}

bool ImporterTexture::Upload(Resource* outResource, IGPUResourceFactory* gpu)
{
    ResourceTexture* texture = down_cast<ResourceTexture*>(outResource);
    if (!gpu->CreateTexture(texture->pixelData.data(), texture))
    {
        NOUS_ERROR("ImporterTexture::Upload() failed for texture '%s'", texture->GetName().c_str());
        return false;
    }
    return true;
}

void ImporterTexture::Release(Resource* inResource, IGPUResourceFactory* gpu)
{
    ResourceTexture* texture = down_cast<ResourceTexture*>(inResource);
    gpu->DestroyTexture(texture);
}

void ImporterTexture::Evict(Resource* inResource)
{
    ResourceTexture* texture = down_cast<ResourceTexture*>(inResource);
    texture->pixelData.clear();
    texture->pixelData.shrink_to_fit();
}
