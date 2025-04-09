#include "VulkanMultithreading.h"

#include "VulkanUtils.h"
#include "Application.h"
#include "NOUS_Multithreading.h"

bool NOUS_VulkanMultithreading::CreateWorkerCommandPools(VulkanContext* vkContext)
{
    if (!vkContext || !vkContext->device.logicalDevice) 
    {
        NOUS_ERROR("Invalid Vulkan context or device");
        return false;
    }

    const auto& threadPool = External->jobSystem->GetThreadPool();
    const auto& threads = threadPool.GetThreads();

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    commandPoolCreateInfo.queueFamilyIndex = vkContext->device.graphicsQueueIndex;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (const auto& thread : threads)
    {
        uint32 threadID = thread->GetID();

        VkCommandPool pool = VK_NULL_HANDLE;

        VK_CHECK(vkCreateCommandPool(
            vkContext->device.logicalDevice,
            &commandPoolCreateInfo,
            vkContext->allocator,
            &pool
        ));

        vkContext->device.workerCommandPools[threadID] = pool;
    }

    return true;
}

bool NOUS_VulkanMultithreading::RecreateWorkerCommandPools(VulkanContext* vkContext)
{
    if (!DestroyWorkerCommandPools(vkContext)) 
    {
        NOUS_ERROR("Failed to destroy old command pools during recreation");
        return false;
    }

    return CreateWorkerCommandPools(vkContext);
}

bool NOUS_VulkanMultithreading::DestroyWorkerCommandPools(VulkanContext* vkContext)
{
    if (vkContext == nullptr || vkContext->device.logicalDevice == VK_NULL_HANDLE)
    {
        NOUS_ERROR("Invalid Vulkan context or logical device");
        return false;
    }

    bool allDestroyed = true;

    for (auto& [threadID, pool] : vkContext->device.workerCommandPools)
    {
        if (pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(
                vkContext->device.logicalDevice,
                pool,
                vkContext->allocator
            );

            pool = VK_NULL_HANDLE;
        }
        else
        {
            NOUS_WARN("Null command pool found for thread %u", threadID);
            allDestroyed = false;
        }
    }

    vkContext->device.workerCommandPools.clear();

    return allDestroyed;
}

VkCommandPool NOUS_VulkanMultithreading::GetThreadCommandPool(VulkanContext* vkContext, uint32 threadID)
{
    try 
    {
        return vkContext->device.workerCommandPools.at(threadID);
    }
    catch (const std::out_of_range&) 
    {
        // Fallback to main command pool
        NOUS_INFO("Worker command pool not found for this worker thread. Falling back to main graphics command pool.");
        return vkContext->device.mainGraphicsCommandPool;
    }
    catch (...) 
    {
        // Catch any other unexpected errors
        NOUS_WARN("Unexpected error accessing worker command pools. Falling back to main graphics pool.");
        return vkContext->device.mainGraphicsCommandPool;
    }
}

bool NOUS_VulkanMultithreading::QueueSubmitThreadSafe(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle)
{
    // If we're on the main thread, submit immediately
    if (NOUS_Multithreading::GetMainThread()->GetID() == NOUS_Multithreading::NOUS_Thread::GetThreadID(std::this_thread::get_id())) 
    {
        std::lock_guard<std::mutex> lock(vkContext->device.graphicsQueueMutex);
        VkResult result = vkQueueSubmit(queue, submitCount, pSubmits, fence);
        if (result != VK_SUCCESS) return false;
        if (waitIdle) return vkQueueWaitIdle(queue) == VK_SUCCESS;
        return true;
    }

    // Otherwise, enqueue the task for the main thread
    VulkanSubmitTask task;
    task.queue = queue;
    task.submitCount = submitCount;
    task.pSubmits = pSubmits;
    task.fence = fence;
    task.waitIdle = waitIdle;

    std::promise<bool> resultPromise;
    auto resultFuture = resultPromise.get_future();
    task.resultPromise = std::move(resultPromise);

    {
        std::lock_guard<std::mutex> lock(vkContext->submitQueueMutex);
        vkContext->submitQueue.push_back(std::move(task));
    }
    vkContext->submitQueueCV.notify_one();

    return resultFuture.get(); // Block until the main thread processes it
}

bool NOUS_VulkanMultithreading::CreateQueueSubmitTask(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle)
{
    // If we're on the main thread, submit immediately
    if (NOUS_Multithreading::GetMainThread()->GetID() == NOUS_Multithreading::NOUS_Thread::GetThreadID(std::this_thread::get_id()))
    {
        std::lock_guard<std::mutex> lock(vkContext->device.graphicsQueueMutex);
        VkResult result = vkQueueSubmit(queue, submitCount, pSubmits, fence);
        if (result != VK_SUCCESS) return false;
        if (waitIdle) return vkQueueWaitIdle(queue) == VK_SUCCESS;
        return true;
    }

    // Otherwise, enqueue the task for the main thread
    VulkanSubmitTask task;
    task.queue = queue;
    task.submitCount = submitCount;
    task.pSubmits = pSubmits;
    task.fence = fence;
    task.waitIdle = waitIdle;

    std::promise<bool> resultPromise;
    auto resultFuture = resultPromise.get_future();
    task.resultPromise = std::move(resultPromise);

    {
        std::lock_guard<std::mutex> lock(vkContext->submitQueueMutex);
        vkContext->submitQueue.push_back(std::move(task));
    }
    vkContext->submitQueueCV.notify_one();

    return resultFuture.get(); // Block until the main thread processes it
}
