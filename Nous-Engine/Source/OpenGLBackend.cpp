#include "OpenGLBackend.h"
#include "OpenGLTypes.inl"

#include "MemoryManager.h"
#include "Logger.h"

OpenGLContext* OpenGLBackend::glContext = nullptr;

OpenGLBackend::OpenGLBackend()
{
	glContext = NOUS_NEW<OpenGLContext>(MemoryManager::MemoryTag::RENDERER);
}

OpenGLBackend::~OpenGLBackend()
{
	NOUS_DELETE(glContext, MemoryManager::MemoryTag::RENDERER);
}

bool OpenGLBackend::Initialize()
{
    bool ret = true;

	NOUS_INFO("USING OPENGL");

    return ret;
}

void OpenGLBackend::Shutdown()
{
}

void OpenGLBackend::Resized(uint16 width, uint16 height)
{
}

bool OpenGLBackend::BeginFrame(float dt)
{
	return false;
}

bool OpenGLBackend::EndFrame(float dt)
{
	return false;
}

void OpenGLBackend::UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{

}

void OpenGLBackend::UpdateObject(float4x4 model)
{

}

void OpenGLBackend::CreateTexture(const char* path, bool autoRelease, int32 width, int32 height, int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture)
{

}

void OpenGLBackend::DestroyTexture(Texture* texture)
{

}