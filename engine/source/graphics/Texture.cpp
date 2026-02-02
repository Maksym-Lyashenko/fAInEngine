#include "graphics/Texture.h"

#include "vk/VkHelpers.h"
#include "Engine.h"

#include <stdexcept>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace eng
{

    static VkCommandBuffer BeginOneTime(VkDevice device, VkCommandPool pool)
    {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        vkutil::vkCheck(vkAllocateCommandBuffers(device, &ai, &cmd), "vkAllocateCommandBuffers failed");

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkutil::vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer failed");
        return cmd;
    }

    static void EndOneTime(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd)
    {
        vkutil::vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer failed");

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        vkutil::vkCheck(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit failed");
        vkutil::vkCheck(vkQueueWaitIdle(queue), "vkQueueWaitIdle failed");

        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    static void CreateImage(VkPhysicalDevice gpu, VkDevice device,
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

        vkutil::vkCheck(vkCreateImage(device, &ci, nullptr, &outImage), "vkCreateImage failed");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device, outImage, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = vkutil::FindMemoryType(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkutil::vkCheck(vkAllocateMemory(device, &ai, nullptr, &outMem), "vkAllocateMemory failed");
        vkutil::vkCheck(vkBindImageMemory(device, outImage, outMem, 0), "vkBindImageMemory failed");
    }

    VkImageView Texture::CreateImageView(VkDevice device, VkImage image, VkFormat format)
    {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image = image;
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = format;
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel = 0;
        iv.subresourceRange.levelCount = m_mipLevels;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount = 1;

        VkImageView view{};
        vkutil::vkCheck(vkCreateImageView(device, &iv, nullptr, &view), "vkCreateImageView failed");
        return view;
    }

    static void TransitionImageLayout(VkCommandBuffer cmd,
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
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            // если появятся новые переходы — лучше явно добавить case
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    static void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t w, uint32_t h)
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

    static bool FormatSupportsLinearBlit(VkPhysicalDevice gpu, VkFormat format)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
        return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    }

    static void GenerateMipmaps(VkPhysicalDevice gpu,
                                VkCommandBuffer cmd,
                                VkImage image,
                                VkFormat format,
                                int32_t texWidth,
                                int32_t texHeight,
                                uint32_t mipLevels)
    {
        if (!FormatSupportsLinearBlit(gpu, format))
        {
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
            return;
        }

        int32_t mipW = texWidth;
        int32_t mipH = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++)
        {
            // mip i-1: DST -> SRC
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

            // mip i: UNDEFINED -> DST
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
                           1, &blit,
                           VK_FILTER_LINEAR);

            // mip i-1: SRC -> SHADER_READ
            TransitionImageLayout(cmd, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

            mipW = nextW;
            mipH = nextH;
        }

        // last mip: DST -> SHADER_READ
        TransitionImageLayout(cmd, image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1);
    }

    void Texture::createSampler()
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_gpu, &props);

        VkPhysicalDeviceFeatures feats{};
        vkGetPhysicalDeviceFeatures(m_gpu, &feats);

        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;

        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        if (feats.samplerAnisotropy)
        {
            si.anisotropyEnable = VK_TRUE;
            si.maxAnisotropy = props.limits.maxSamplerAnisotropy;
        }
        else
        {
            si.anisotropyEnable = VK_FALSE;
            si.maxAnisotropy = 1.0f;
        }

        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.minLod = 0.0f;
        si.maxLod = (float)(m_mipLevels > 0 ? (m_mipLevels - 1) : 0);
        si.mipLodBias = 0.0f;

        vkutil::vkCheck(vkCreateSampler(m_device, &si, nullptr, &m_sampler), "vkCreateSampler failed");
    }

    bool Texture::LoadFromFile(VkPhysicalDevice gpu,
                               VkDevice device,
                               VkQueue graphicsQueue,
                               VkCommandPool cmdPool,
                               const std::filesystem::path &path,
                               bool srgb)
    {
        m_gpu = gpu;
        m_device = device;

        int w = 0, h = 0, comp = 0;
        stbi_uc *pixels = stbi_load(path.string().c_str(), &w, &h, &comp, STBI_rgb_alpha);
        if (!pixels)
            return false;

        m_width = (uint32_t)w;
        m_height = (uint32_t)h;

        m_format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        uint32_t desiredMip = 1u + (uint32_t)std::floor(std::log2(std::max(m_width, m_height)));
        if (!FormatSupportsLinearBlit(m_gpu, m_format))
            desiredMip = 1;
        m_mipLevels = desiredMip;

        const VkDeviceSize imageSize = (VkDeviceSize)m_width * (VkDeviceSize)m_height * 4;

        // staging buffer
        VkBuffer stagingBuf{};
        VkDeviceMemory stagingMem{};
        vkutil::CreateBuffer(m_gpu, m_device, imageSize,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             stagingBuf, stagingMem);

        void *mapped = nullptr;
        vkutil::vkCheck(vkMapMemory(m_device, stagingMem, 0, imageSize, 0, &mapped), "vkMapMemory failed");
        std::memcpy(mapped, pixels, (size_t)imageSize);
        vkUnmapMemory(m_device, stagingMem);

        stbi_image_free(pixels);

        // device image
        CreateImage(m_gpu, m_device, m_width, m_height, m_mipLevels, m_format,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    m_image, m_memory);

        VkCommandBuffer cmd = BeginOneTime(m_device, cmdPool);
        TransitionImageLayout(cmd, m_image,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                              0, 1);
        CopyBufferToImage(cmd, stagingBuf, m_image, m_width, m_height);
        GenerateMipmaps(m_gpu, cmd, m_image, m_format, (int32_t)m_width, (int32_t)m_height, m_mipLevels);
        EndOneTime(m_device, graphicsQueue, cmdPool, cmd);

        vkDestroyBuffer(m_device, stagingBuf, nullptr);
        vkFreeMemory(m_device, stagingMem, nullptr);

        m_view = CreateImageView(m_device, m_image, m_format);
        createSampler();
        return true;
    }

    std::shared_ptr<Texture> Texture::Load(VkPhysicalDevice gpu,
                                           VkDevice device,
                                           VkQueue graphicsQueue,
                                           VkCommandPool cmdPool,
                                           const std::string &path)
    {
        auto tex = std::make_shared<Texture>();

        auto &fs = Engine::GetInstance().GetFileSystem();
        auto fullPath = fs.GetAssetsFolder() / path;

        if (!tex->LoadFromFile(gpu, device, graphicsQueue, cmdPool, fullPath, true))
            return nullptr;
        return tex;
    }

    void Texture::Destroy()
    {
        if (!m_device)
            return;

        if (m_sampler)
        {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
        if (m_view)
        {
            vkDestroyImageView(m_device, m_view, nullptr);
            m_view = VK_NULL_HANDLE;
        }
        if (m_image)
        {
            vkDestroyImage(m_device, m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }
        if (m_memory)
        {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }

        m_device = VK_NULL_HANDLE;
        m_gpu = VK_NULL_HANDLE;
    }

    std::shared_ptr<Texture> TextureManager::GetOrLoadTexture(const std::string &path)
    {
        const std::string key = std::filesystem::path(path).lexically_normal().generic_string();

        if (auto it = m_textures.find(key); it != m_textures.end())
        {
            if (auto sp = it->second.lock())
                return sp;
        }

        auto &vk = Engine::GetInstance().GetVulkanContext();
        auto tex = Texture::Load(vk.GetGPU(), vk.GetDevice(), vk.GetGraphicsQueue(), vk.GetCommandPool(), key);

        if (!tex)
        {
            SDL_Log("TextureManager: failed to load '%s'", key.c_str());
            return nullptr;
        }

        m_textures[key] = tex;
        return tex;
    }

}