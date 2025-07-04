#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"

#include "MemoryManager.h"
#include "VulkanMultithreading.h"

bool NOUS_VulkanCommandBuffer::CreateCommandBuffers(VulkanContext* vkContext)
{
    bool ret = true;

    // Swap Chain

    if (vkContext->graphicsCommandBuffers.empty()) 
    {
        // Careful here, maybe we should use MAX_FRAMES_IN_FLIGHT
        vkContext->graphicsCommandBuffers.resize(vkContext->swapChain.swapChainImages.size());
        MemoryManager::ZeroMemory(vkContext->graphicsCommandBuffers.data(), vkContext->graphicsCommandBuffers.size() * sizeof(VulkanCommandBuffer));
    }

    for (auto it = vkContext->graphicsCommandBuffers.begin(); it != vkContext->graphicsCommandBuffers.end(); ++it)
    {
        if ((*it).handle)
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }

        MemoryManager::ZeroMemory(&(*it), sizeof(VulkanCommandBuffer));
        CommandBufferAllocate(vkContext, vkContext->device.mainGraphicsCommandPool, true, &(*it));
    }

    // Scene Viewport

    if (vkContext->imGuiResources.m_ViewportCommandBuffers.empty())
    {
        // Careful here, maybe we should use MAX_FRAMES_IN_FLIGHT
        vkContext->imGuiResources.m_ViewportCommandBuffers.resize(vkContext->imGuiResources.m_ViewportImages.size());
        MemoryManager::ZeroMemory(vkContext->imGuiResources.m_ViewportCommandBuffers.data(), vkContext->imGuiResources.m_ViewportCommandBuffers.size() * sizeof(VulkanCommandBuffer));
    }

    for (auto it = vkContext->imGuiResources.m_ViewportCommandBuffers.begin(); it != vkContext->imGuiResources.m_ViewportCommandBuffers.end(); ++it)
    {
        if ((*it).handle)
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }

        MemoryManager::ZeroMemory(&(*it), sizeof(VulkanCommandBuffer));
        CommandBufferAllocate(vkContext, vkContext->device.mainGraphicsCommandPool, true, &(*it));
    }

    // Game Viewport

    if (vkContext->imGuiResources.m_GameViewportCommandBuffers.empty())
    {
        // Careful here, maybe we should use MAX_FRAMES_IN_FLIGHT
        vkContext->imGuiResources.m_GameViewportCommandBuffers.resize(vkContext->imGuiResources.m_GameViewportImages.size());
        MemoryManager::ZeroMemory(vkContext->imGuiResources.m_GameViewportCommandBuffers.data(), vkContext->imGuiResources.m_GameViewportCommandBuffers.size() * sizeof(VulkanCommandBuffer));
    }

    for (auto it = vkContext->imGuiResources.m_GameViewportCommandBuffers.begin(); it != vkContext->imGuiResources.m_GameViewportCommandBuffers.end(); ++it)
    {
        if ((*it).handle)
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }

        MemoryManager::ZeroMemory(&(*it), sizeof(VulkanCommandBuffer));
        CommandBufferAllocate(vkContext, vkContext->device.mainGraphicsCommandPool, true, &(*it));
    }

    return ret;
}

void NOUS_VulkanCommandBuffer::DestroyCommandBuffers(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Command Buffers...");

    // Swap Chain

    for (auto it = vkContext->graphicsCommandBuffers.rbegin(); it != vkContext->graphicsCommandBuffers.rend(); ++it) 
    {
        if ((*it).handle) 
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }   
    }

    vkContext->graphicsCommandBuffers.clear();
    vkContext->graphicsCommandBuffers.shrink_to_fit();

    // Scene Viewport

    for (auto it = vkContext->imGuiResources.m_ViewportCommandBuffers.rbegin(); it != vkContext->imGuiResources.m_ViewportCommandBuffers.rend(); ++it)
    {
        if ((*it).handle)
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }
    }

    vkContext->imGuiResources.m_ViewportCommandBuffers.clear();
    vkContext->imGuiResources.m_ViewportCommandBuffers.shrink_to_fit();

    // Game Viewport

    for (auto it = vkContext->imGuiResources.m_GameViewportCommandBuffers.rbegin(); it != vkContext->imGuiResources.m_GameViewportCommandBuffers.rend(); ++it)
    {
        if ((*it).handle)
        {
            CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &(*it));
            (*it).handle = 0;
        }
    }

    vkContext->imGuiResources.m_GameViewportCommandBuffers.clear();
    vkContext->imGuiResources.m_GameViewportCommandBuffers.shrink_to_fit();
}

void NOUS_VulkanCommandBuffer::CommandBufferAllocate(VulkanContext* vkContext, VkCommandPool commandPool,
	bool isPrimary, VulkanCommandBuffer* outCommandBuffer)
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{}; 
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.pNext = 0;

    outCommandBuffer->state = VulkanCommandBufferState::NOT_ALLOCATED;

    VK_CHECK(vkAllocateCommandBuffers(vkContext->device.logicalDevice, &commandBufferAllocateInfo, &outCommandBuffer->handle));

    outCommandBuffer->state = VulkanCommandBufferState::READY;
}

void NOUS_VulkanCommandBuffer::CommandBufferFree(VulkanContext* vkContext, VkCommandPool commandPool,
	VulkanCommandBuffer* commandBuffer)
{
    vkFreeCommandBuffers(vkContext->device.logicalDevice, commandPool, 1, &commandBuffer->handle);
    commandBuffer->handle = 0;

    commandBuffer->state = VulkanCommandBufferState::NOT_ALLOCATED;
}

void NOUS_VulkanCommandBuffer::CommandBufferBegin(VulkanCommandBuffer* commandBuffer, bool isSingleUse, 
	bool isRenderpassContinue, bool isSimultaneousUse)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    commandBufferBeginInfo.flags = 0;

    if (isSingleUse) 
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    if (isRenderpassContinue)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    if (isSimultaneousUse)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }

    VK_CHECK(vkBeginCommandBuffer(commandBuffer->handle, &commandBufferBeginInfo));
    commandBuffer->state = VulkanCommandBufferState::RECORDING;
}

void NOUS_VulkanCommandBuffer::CommandBufferEnd(VulkanCommandBuffer* commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer->handle));
    commandBuffer->state = VulkanCommandBufferState::RECORDING_ENDED;
}

void NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(VulkanCommandBuffer* commandBuffer)
{
    commandBuffer->state = VulkanCommandBufferState::SUBMITTED;
}

void NOUS_VulkanCommandBuffer::CommandBufferReset(VulkanCommandBuffer* commandBuffer)
{
    commandBuffer->state = VulkanCommandBufferState::READY;
}

void NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(VulkanContext* vkContext, 
	VkCommandPool commandPool, VulkanCommandBuffer* outCommandBuffer)
{
    CommandBufferAllocate(vkContext, commandPool, true, outCommandBuffer);
    CommandBufferBegin(outCommandBuffer, true, false, false);
}

void NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(VulkanContext* vkContext, VkCommandPool commandPool,
	VulkanCommandBuffer* commandBuffer, VkQueue queue)
{
    // End the command buffer.
    CommandBufferEnd(commandBuffer);

    // Submit the queue
    VkSubmitInfo queueSubmitInfo{};
    queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    queueSubmitInfo.commandBufferCount = 1;
    queueSubmitInfo.pCommandBuffers = &commandBuffer->handle;

    NOUS_VulkanMultithreading::CreateQueueSubmitTask(vkContext, queue, 1, &queueSubmitInfo, 0, true);

    // Free the command buffer.
    CommandBufferFree(vkContext, commandPool, commandBuffer);
}