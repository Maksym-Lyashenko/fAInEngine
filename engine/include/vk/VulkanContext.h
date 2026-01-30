#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include "render/Material.h"

#include <vector>
#include <optional>
#include <cstdint>
#include <string>

namespace eng
{

    class Swapchain
    {
    public:
        Swapchain() = default;
        ~Swapchain();

        Swapchain(const Swapchain &) = delete;
        Swapchain &operator=(const Swapchain &) = delete;

        void create(VkPhysicalDevice gpu,
                    VkDevice device,
                    VkSurfaceKHR surface,
                    SDL_Window *window,
                    uint32_t qGraphics,
                    uint32_t qPresent);

        void destroy();
        void recreate(SDL_Window *window);

        VkSwapchainKHR handle() const { return m_swapchain; }
        VkFormat format() const { return m_format; }
        VkExtent2D extent() const { return m_extent; }

        size_t imageCount() const { return m_images.size(); }
        VkFramebuffer framebuffer(size_t i) const { return m_framebuffers[i]; }
        VkRenderPass renderPass() const { return m_renderPass; }

        static bool hasAdequateSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);

    private:
        struct Support
        {
            VkSurfaceCapabilitiesKHR caps{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        static Support querySupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);
        static VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR> &formats);
        static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &modes);
        static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &caps, SDL_Window *window);

        void createImageViews();
        void createRenderPass();
        void createFramebuffers();

    private:
        VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        uint32_t m_qGraphics = 0;
        uint32_t m_qPresent = 0;

        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        VkFormat m_format{};
        VkExtent2D m_extent{};

        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_views;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_framebuffers;
    };

    class CommandPool
    {
    public:
        CommandPool() = default;
        ~CommandPool();

        CommandPool(const CommandPool &) = delete;
        CommandPool &operator=(const CommandPool &) = delete;

        void create(VkDevice device, uint32_t queueFamilyIndex);
        void destroy();

        void reset(); // vkResetCommandPool
        void allocate(uint32_t count);

        VkCommandBuffer at(size_t i) const { return m_cmdBufs[i]; }
        size_t size() const { return m_cmdBufs.size(); }
        VkCommandPool handle() const { return m_pool; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_cmdBufs;
    };

    class FrameSync
    {
    public:
        static constexpr int MAX_FRAMES = 2;

        FrameSync() = default;
        ~FrameSync();

        FrameSync(const FrameSync &) = delete;
        FrameSync &operator=(const FrameSync &) = delete;

        void create(VkDevice device);
        void destroy();

        uint32_t frameIndex() const { return m_frameIndex; }
        void advance() { m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES; }

        VkSemaphore imageAvailable() const { return m_imageAvailable[m_frameIndex]; }
        VkSemaphore renderFinished() const { return m_renderFinished[m_frameIndex]; }
        VkFence inFlightFence() const { return m_inFlight[m_frameIndex]; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSemaphore m_imageAvailable[MAX_FRAMES]{};
        VkSemaphore m_renderFinished[MAX_FRAMES]{};
        VkFence m_inFlight[MAX_FRAMES]{};
        uint32_t m_frameIndex = 0;
    };

    class VulkanContext
    {
    public:
        VulkanContext() = default;
        ~VulkanContext();

        VulkanContext(const VulkanContext &) = delete;
        VulkanContext &operator=(const VulkanContext &) = delete;

        void init(SDL_Window *window);
        void drawFrame(SDL_Window *window, bool resized);
        void waitIdle();

        void RegisterShaderProgram(const std::shared_ptr<ShaderProgram> &sp);
        void RecreateAllPrograms();

    public:
        VkDevice GetDevice() const { return m_device; }
        VkPhysicalDevice GetGPU() const { return m_gpu; }
        VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
        VkCommandPool GetCommandPool() const { return m_cmdPool.handle(); }

        VkRenderPass GetRenderPass() const { return m_swapchain.renderPass(); }
        VkExtent2D GetExtent() const { return m_swapchain.extent(); }

    private:
        struct QueueFamilies
        {
            std::optional<uint32_t> graphics;
            std::optional<uint32_t> present;
            bool complete() const { return graphics.has_value() && present.has_value(); }
        };

        void createInstance(SDL_Window *window);
        void setupDebugMessenger();
        void createSurface(SDL_Window *window);
        void pickPhysicalDevice();
        void createDevice();

        void recordCommandBuffer(uint32_t imageIndex);

        void recreateSwapchain(SDL_Window *window);

        static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface);
        static bool hasDeviceExtension(VkPhysicalDevice gpu, const char *extName);
        static bool checkValidationLayerSupport();

        std::vector<VkSemaphore> m_renderFinishedPerImage; // size = swapchain image count
        std::vector<VkFence> m_imagesInFlight;             // size = swapchain image count (or VK_NULL_HANDLE)

        void createPerImageSync();
        void destroyPerImageSync();

        std::vector<std::shared_ptr<ShaderProgram>> m_programs;

    private:
        // Debug toggles (validation only in Debug)
        static constexpr bool kEnableValidation =
#ifndef NDEBUG
            true;
#else
            false;
#endif

        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;

        uint32_t m_qGraphics = 0;
        uint32_t m_qPresent = 0;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;

        Swapchain m_swapchain;
        CommandPool m_cmdPool;
        FrameSync m_sync;

        bool m_framebufferResized = false;
    };

}
