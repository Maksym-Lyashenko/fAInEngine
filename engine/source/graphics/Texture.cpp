#include "graphics/Texture.h"

#include "vk/VkHelpers.h"
#include "Engine.h"

#include <stdexcept>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace eng
{
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

    static bool FormatSupportsLinearBlit(VkPhysicalDevice gpu, VkFormat format)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
        return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
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

        si.anisotropyEnable = feats.samplerAnisotropy ? VK_TRUE : VK_FALSE;
        si.maxAnisotropy = feats.samplerAnisotropy ? props.limits.maxSamplerAnisotropy : 1.0f;

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
        vkutil::CreateImage(m_gpu, m_device, m_width, m_height, m_mipLevels, m_format,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            m_image, m_memory);

        VkCommandBuffer cmd = vkutil::BeginOneTime(m_device, cmdPool);
        vkutil::TransitionImageLayout(cmd, m_image,
                                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                      0, 1);
        vkutil::CopyBufferToImage(cmd, stagingBuf, m_image, m_width, m_height);
        vkutil::GenerateMipmaps(m_gpu, cmd, m_image, m_format, (int32_t)m_width, (int32_t)m_height, m_mipLevels);
        vkutil::EndOneTime(m_device, graphicsQueue, cmdPool, cmd);

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