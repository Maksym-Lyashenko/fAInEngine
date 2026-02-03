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

    // ---- memory/buffer ----
    uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeBits, VkMemoryPropertyFlags props);

    void CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                      VkBuffer &outBuf, VkDeviceMemory &outMem);

    // One-time command helpers (upload copies etc.)
    VkCommandBuffer BeginOneTime(VkDevice device, VkCommandPool pool);
    void EndOneTime(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd);

    void CopyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool,
                    VkBuffer src, VkBuffer dst, VkDeviceSize size);

    // ---- image helpers (for textures/mips) ----
    void CreateImage(VkPhysicalDevice gpu, VkDevice device,
                     uint32_t w, uint32_t h, uint32_t mipLevels,
                     VkFormat format, VkImageUsageFlags usage,
                     VkImage &outImage, VkDeviceMemory &outMem);

    VkImageView CreateImageView(VkDevice device, VkImage image,
                                VkFormat format, VkImageAspectFlags aspect,
                                uint32_t baseMip, uint32_t mipCount);

    VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice gpu);

    void TransitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               VkImageAspectFlags aspect,
                               uint32_t baseMip,
                               uint32_t levelCount);

    void CopyBufferToImage(VkCommandBuffer cmd,
                           VkBuffer buffer, VkImage image,
                           uint32_t w, uint32_t h);

    bool FormatSupportsLinearBlit(VkPhysicalDevice gpu, VkFormat format);

    void GenerateMipmaps(VkPhysicalDevice gpu, VkCommandBuffer cmd,
                         VkImage image, VkFormat format,
                         int32_t texWidth, int32_t texHeight,
                         uint32_t mipLevels);
}
