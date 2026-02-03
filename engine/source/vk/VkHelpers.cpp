#include "vk/VkHelpers.h"
#include <algorithm>
#include <cmath>

namespace eng::vkutil
{
    uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(gpu, &mp);

        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        {
            if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        throw std::runtime_error("FindMemoryType: no suitable memory type");
    }

    void CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                      VkBuffer &outBuf, VkDeviceMemory &outMem)
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCheck(vkCreateBuffer(device, &bi, nullptr, &outBuf), "vkCreateBuffer failed");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, outBuf, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(gpu, req.memoryTypeBits, memProps);

        vkCheck(vkAllocateMemory(device, &ai, nullptr, &outMem), "vkAllocateMemory failed");
        vkCheck(vkBindBufferMemory(device, outBuf, outMem, 0), "vkBindBufferMemory failed");
    }

    VkCommandBuffer BeginOneTime(VkDevice device, VkCommandPool pool)
    {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkCheck(vkAllocateCommandBuffers(device, &ai, &cmd), "vkAllocateCommandBuffers failed");

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer failed");

        return cmd;
    }

    void EndOneTime(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd)
    {
        vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer failed");

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence = VK_NULL_HANDLE;
        vkCheck(vkCreateFence(device, &fci, nullptr, &fence), "vkCreateFence failed");

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        vkCheck(vkQueueSubmit(queue, 1, &si, fence), "vkQueueSubmit failed");
        vkCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences failed");

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    void CopyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool,
                    VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBuffer cmd = BeginOneTime(device, pool);

        VkBufferCopy copy{};
        copy.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy);

        EndOneTime(device, queue, pool, cmd);
    }

    void CreateImage(VkPhysicalDevice gpu, VkDevice device,
                     uint32_t w, uint32_t h, uint32_t mipLevels,
                     VkFormat format, VkImageUsageFlags usage,
                     VkImage &outImage, VkDeviceMemory &outMem)
    {
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.extent = {w, h, 1};
        ci.mipLevels = mipLevels;
        ci.arrayLayers = 1;
        ci.format = format;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci.usage = usage;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCheck(vkCreateImage(device, &ci, nullptr, &outImage), "vkCreateImage failed");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device, outImage, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkCheck(vkAllocateMemory(device, &ai, nullptr, &outMem), "vkAllocateMemory failed");
        vkCheck(vkBindImageMemory(device, outImage, outMem, 0), "vkBindImageMemory failed");
    }

    VkImageView CreateImageView(VkDevice device, VkImage image,
                                VkFormat format, VkImageAspectFlags aspect,
                                uint32_t baseMip, uint32_t mipCount)
    {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image = image;
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = format;
        iv.subresourceRange.aspectMask = aspect;
        iv.subresourceRange.baseMipLevel = baseMip;
        iv.subresourceRange.levelCount = mipCount;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        vkCheck(vkCreateImageView(device, &iv, nullptr, &view), "vkCreateImageView failed");
        return view;
    }

    VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice gpu)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpu, &props);

        VkSampleCountFlags counts =
            props.limits.framebufferColorSampleCounts &
            props.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT)
            return VK_SAMPLE_COUNT_64_BIT;
        if (counts & VK_SAMPLE_COUNT_32_BIT)
            return VK_SAMPLE_COUNT_32_BIT;
        if (counts & VK_SAMPLE_COUNT_16_BIT)
            return VK_SAMPLE_COUNT_16_BIT;
        if (counts & VK_SAMPLE_COUNT_8_BIT)
            return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT)
            return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT)
            return VK_SAMPLE_COUNT_2_BIT;

        return VK_SAMPLE_COUNT_1_BIT;
    }

    void TransitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               VkImageAspectFlags aspect,
                               uint32_t baseMip,
                               uint32_t levelCount)
    {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = baseMip;
        barrier.subresourceRange.levelCount = levelCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void CopyBufferToImage(VkCommandBuffer cmd,
                           VkBuffer buffer, VkImage image,
                           uint32_t w, uint32_t h)
    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {w, h, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    bool FormatSupportsLinearBlit(VkPhysicalDevice gpu, VkFormat format)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
        return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    }

    void GenerateMipmaps(VkPhysicalDevice gpu, VkCommandBuffer cmd,
                         VkImage image, VkFormat format,
                         int32_t texWidth, int32_t texHeight,
                         uint32_t mipLevels)
    {
        if (mipLevels <= 1)
        {
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
            return;
        }

        if (!FormatSupportsLinearBlit(gpu, format))
        {
            // Fallback: без mipmaps, чтобы sampler не лез в несуществующие уровни
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
            return;
        }

        int32_t mipW = texWidth;
        int32_t mipH = texHeight;

        for (uint32_t i = 1; i < mipLevels; ++i)
        {
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, i, 1);

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipW, mipH, 1};

            int32_t nextW = std::max(1, mipW / 2);
            int32_t nextH = std::max(1, mipH / 2);

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {nextW, nextH, 1};

            vkCmdBlitImage(cmd,
                           image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

            mipW = nextW;
            mipH = nextH;
        }

        TransitionImageLayout(cmd, image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1);
    }
}
