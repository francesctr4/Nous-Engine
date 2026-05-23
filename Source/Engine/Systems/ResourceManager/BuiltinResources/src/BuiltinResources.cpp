#include "Engine/Systems/ResourceManager/BuiltinResources/include/BuiltinResources.h"

#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

std::vector<std::pair<ResourceType, Resource*>> BuiltinResources::Create()
{
    // -----------------------------------------------------------------------
    // Textures
    // Reserved UIDs sit at the top of the uint32 range so they cannot collide
    // with randomly-generated asset UIDs in .meta files.
    // These UIDs are what the descriptor lazy-write dedup (WriteInstanceSampler)
    // keys off via Resource::GetUID(), so each fallback must be unique.
    // -----------------------------------------------------------------------

    // Default Texture — 256×256 checkerboard
    constexpr uint32 texDimension = 256;
    constexpr uint32 channels     = 4;
    constexpr uint32 pixelCount   = texDimension * texDimension;

    m_defaultTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
    m_defaultTexture->SetUID(INVALID_ID - 1);
    m_defaultTexture->SetName("DefaultTexture");
    m_defaultTexture->width        = texDimension;
    m_defaultTexture->height       = texDimension;
    m_defaultTexture->channelCount = channels;
    m_defaultTexture->pixelData.resize(pixelCount * channels, 255);
    for (uint32_t row = 0; row < texDimension; ++row)
    {
        for (uint32_t col = 0; col < texDimension; ++col)
        {
            constexpr uint32 squareSize = 16;
            const uint32_t   indexBpp   = (row * texDimension + col) * channels;
            const bool       isWhite    = row / squareSize % 2 == col / squareSize % 2;
            m_defaultTexture->pixelData[indexBpp + 0] = isWhite ? 255 : 0;
            m_defaultTexture->pixelData[indexBpp + 1] = isWhite ? 255 : 0;
            m_defaultTexture->pixelData[indexBpp + 2] = 255;
            m_defaultTexture->pixelData[indexBpp + 3] = 255;
        }
    }
    m_defaultTexture->SetState(ResourceState::CPU_READY);

    // White Texture — 1×1 (1,1,1,1). Neutral identity for multiplicative slots.
    m_whiteTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
    m_whiteTexture->SetUID(INVALID_ID - 2);
    m_whiteTexture->SetName("WhiteTexture");
    m_whiteTexture->width        = 1;
    m_whiteTexture->height       = 1;
    m_whiteTexture->channelCount = 4;
    m_whiteTexture->pixelData    = { 255, 255, 255, 255 };
    m_whiteTexture->SetState(ResourceState::CPU_READY);

    // Black Texture — 1×1 (0,0,0,1). Neutral identity for additive slots (emissive).
    m_blackTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
    m_blackTexture->SetUID(INVALID_ID - 3);
    m_blackTexture->SetName("BlackTexture");
    m_blackTexture->width        = 1;
    m_blackTexture->height       = 1;
    m_blackTexture->channelCount = 4;
    m_blackTexture->pixelData    = { 0, 0, 0, 255 };
    m_blackTexture->SetState(ResourceState::CPU_READY);

    // Flat Normal Texture — 1×1 tangent-space flat normal (128,128,255,255).
    // Decoded as (0,0,1) in [-1,1]. Using white would produce a 45° tilt.
    m_flatNormalTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
    m_flatNormalTexture->SetUID(INVALID_ID - 4);
    m_flatNormalTexture->SetName("FlatNormalTexture");
    m_flatNormalTexture->width        = 1;
    m_flatNormalTexture->height       = 1;
    m_flatNormalTexture->channelCount = 4;
    m_flatNormalTexture->pixelData    = { 128, 128, 255, 255 };
    m_flatNormalTexture->SetState(ResourceState::CPU_READY);

    // -----------------------------------------------------------------------
    // Default Material
    // -----------------------------------------------------------------------
    using enum UniformValueType;

    m_defaultMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
    m_defaultMaterial->SetName("DefaultMaterial");
    m_defaultMaterial->uniformValues["diffuseColor"]      = { Vec4,  glm::vec4(1.0f) };
    m_defaultMaterial->uniformValues["emissiveColor"]     = { Vec4,  glm::vec4(1.0f) };
    m_defaultMaterial->uniformValues["aoIntensity"]       = { Float, glm::vec4(1.0f) };
    m_defaultMaterial->uniformValues["normalStrength"]    = { Float, glm::vec4(1.0f) };
    m_defaultMaterial->uniformValues["specularIntensity"] = { Float, glm::vec4(1.0f) };
    m_defaultMaterial->uniformValues["shininessScale"]    = { Float, glm::vec4(1.0f) };
    m_defaultMaterial->textureMaps["diffuseSampler"].texture = m_defaultTexture;
    m_defaultMaterial->SetState(ResourceState::CPU_READY);

    // Textures must precede the material in the upload queue so they are
    // GPU_READY before the material's descriptor set is first written.
    using enum ResourceType;
    return {
        { TEXTURE,  m_defaultTexture    },
        { TEXTURE,  m_whiteTexture      },
        { TEXTURE,  m_blackTexture      },
        { TEXTURE,  m_flatNormalTexture },
        { MATERIAL, m_defaultMaterial   },
    };
}

void BuiltinResources::Destroy(IGPUResourceFactory* gpu)
{
    DestroyTexture(m_defaultTexture,    gpu);
    DestroyTexture(m_whiteTexture,      gpu);
    DestroyTexture(m_blackTexture,      gpu);
    DestroyTexture(m_flatNormalTexture, gpu);

    if (m_defaultMaterial)
    {
        if (m_defaultMaterial->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(m_defaultMaterial);
        NOUS_DELETE(m_defaultMaterial, MemoryTag::RESOURCE_MATERIAL);
        m_defaultMaterial = nullptr;
    }
}

void BuiltinResources::DestroyTexture(ResourceTexture*& tex, IGPUResourceFactory* gpu)
{
    if (!tex) return;
    if (tex->GetState() == ResourceState::GPU_READY)
        gpu->DestroyTexture(tex);
    NOUS_DELETE(tex, MemoryTag::RESOURCE_TEXTURE);
    tex = nullptr;
}

ResourceTexture*  BuiltinResources::GetDefaultTexture()    const { return m_defaultTexture;    }
ResourceTexture*  BuiltinResources::GetWhiteTexture()      const { return m_whiteTexture;      }
ResourceTexture*  BuiltinResources::GetBlackTexture()      const { return m_blackTexture;      }
ResourceTexture*  BuiltinResources::GetFlatNormalTexture() const { return m_flatNormalTexture; }
ResourceMaterial* BuiltinResources::GetDefaultMaterial()   const { return m_defaultMaterial;  }
