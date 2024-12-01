#include "ImporterTexture.h"
#include "RendererFrontend.h"

#define STB_IMAGE_IMPLEMENTATION
#include "External/stb_image/stb_image.h"

bool ImporterTexture::Import(const char* path, Texture* texture)
{
    const int32 requiredChannelCount = 4;

    stbi_set_flip_vertically_on_load(true);

    // Use a temporary texture to load into.
    Texture tempTexture;

    uint8* data = stbi_load(path, (int32*)&tempTexture.width, (int32*)&tempTexture.height, 
        (int32*)&tempTexture.channelCount, requiredChannelCount);

    tempTexture.channelCount = requiredChannelCount;

    if (data) 
    {
        uint32 currentGeneration = texture->generation;
        texture->generation = INVALID_ID;

        uint64 totalSize = tempTexture.width * tempTexture.height * requiredChannelCount;

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
            NOUS_WARN("load_texture() failed to load file '%s': %s", path, stbi_failure_reason());
        }

        // Acquire internal texture resources and upload to GPU.
        CreateTexture(textureName, true, temp_texture.width, temp_texture.height, 
            temp_texture.channel_count, data, has_transparency, &temp_texture);

        // Take a copy of the old texture.
        Texture old = *texture;

        // Assign the temp texture to the pointer.
        *texture = tempTexture;

        // Destroy the old texture.
        DestroyTexture(&old);

        if (currentGeneration == INVALID_ID) 
        {
            texture->generation = 0;
        }
        else 
        {
            texture->generation = currentGeneration + 1;
        }

        // Clean up data.
        stbi_image_free(data);

        return true;
    }
    else 
    {
        if (stbi_failure_reason()) 
        {
            NOUS_WARN("load_texture() failed to load file '%s': %s", path, stbi_failure_reason());
        }

        return false;
    }
}
