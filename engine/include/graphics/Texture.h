#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <filesystem>
#include <unordered_map>

namespace eng
{

    class Texture
    {
    public:
        Texture() = default;
        ~Texture() { Destroy(); }

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        // Load via stb_image (RGBA8)
        bool LoadFromFile(VkPhysicalDevice gpu,
                          VkDevice device,
                          VkQueue graphicsQueue,
                          VkCommandPool cmdPool,
                          const std::filesystem::path &path,
                          bool srgb);

        static std::shared_ptr<Texture> Load(VkPhysicalDevice gpu, VkDevice device,
                                             VkQueue graphicsQueue, VkCommandPool cmdPool,
                                             const std::string &path);

        void Destroy();

        VkImageView View() const { return m_view; }
        VkSampler Sampler() const { return m_sampler; }

    private:
        void createSampler();

        VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format);

    private:
        VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
        VkFormat m_format = VK_FORMAT_R8G8B8A8_SRGB; // good default for color textures
    };

    class TextureManager
    {
    public:
        std::shared_ptr<Texture> GetOrLoadTexture(const std::string &path);

    private:
        std::unordered_map<std::string, std::weak_ptr<Texture>> m_textures;
    };

}
