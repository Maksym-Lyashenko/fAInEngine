#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>

namespace eng::vkutil
{

    inline void vkCheck(VkResult r, const char *msg)
    {
        if (r != VK_SUCCESS)
            throw std::runtime_error(msg);
    }

    uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeBits, VkMemoryPropertyFlags props);

    void CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                      VkBuffer &outBuf, VkDeviceMemory &outMem);

    void CopyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool,
                    VkBuffer src, VkBuffer dst, VkDeviceSize size);

}
