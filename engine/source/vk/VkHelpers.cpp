#include "vk/VkHelpers.h"

namespace eng::vkutil
{

    uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(gpu, &mp);
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
                return i;
        throw std::runtime_error("No suitable memory type");
    }

    void CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                      VkBuffer &outBuf, VkDeviceMemory &outMem)
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkutil::vkCheck(vkCreateBuffer(device, &bi, nullptr, &outBuf), "vkCreateBuffer failed");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, outBuf, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = vkutil::FindMemoryType(gpu, req.memoryTypeBits, memProps);
        vkutil::vkCheck(vkAllocateMemory(device, &ai, nullptr, &outMem), "vkAllocateMemory failed");

        vkutil::vkCheck(vkBindBufferMemory(device, outBuf, outMem, 0), "vkBindBufferMemory failed");
    }

    void CopyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool, VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool = pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        vkAllocateCommandBuffers(device, &cai, &cmd);

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        VkBufferCopy copy{};
        copy.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

}