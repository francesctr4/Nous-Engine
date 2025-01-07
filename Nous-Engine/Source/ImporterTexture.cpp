#include "ImporterTexture.h"
#include "RendererFrontend.h"
#include "FileHandle.h"
#include "FileManager.h"

#include "ModuleRenderer3D.h"
#include "MetaFileData.inl"

#include "ResourceTexture.h"
#include "MemoryManager.h"

#define STB_IMAGE_IMPLEMENTATION
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

    stbi_set_flip_vertically_on_load(true);

    uint8* data = stbi_load(libraryPath.c_str(), (int32*)&texture->width, (int32*)&texture->height,
        (int32*)&texture->channelCount, requiredChannelCount);

    texture->channelCount = requiredChannelCount;

    if (data)
    {
        uint32 currentGeneration = texture->GetReferenceCount() == 0 ? INVALID_ID : texture->GetReferenceCount();

        uint64 totalSize = texture->width * texture->height * requiredChannelCount;

        // Check for transparency
        bool hasTransparency = false;

        for (uint64 i = 0; i < totalSize; i += requiredChannelCount)
        {
            uint8 a = data[i + 3];

            if (a < 255)
            {
                hasTransparency = true;
                break;
            }
        }

        if (stbi_failure_reason())
        {
            NOUS_WARN("ImporterTexture::Load() failed to load file '%s': %s", libraryPath, stbi_failure_reason());
        }

        // IDK what to do with this
        if (currentGeneration == INVALID_ID)
        {
            texture->generation = 0;
        }
        else
        {
            texture->generation = currentGeneration;
        }

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
            NOUS_WARN("ImporterTexture::Load() failed to load file '%s': %s", libraryPath, stbi_failure_reason());
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
