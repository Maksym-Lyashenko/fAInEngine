#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

namespace eng
{
    class ShaderProgram;
    struct CameraData;

    // ---------------- Swapchain ----------------
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
                    uint32_t qPresent,
                    VkSampleCountFlagBits msaaSamples);

        void destroy();
        void recreate(SDL_Window *window);

        VkSwapchainKHR handle() const { return m_swapchain; }
        VkFormat format() const { return m_format; }
        VkExtent2D extent() const { return m_extent; }

        VkFormat depthFormat() const { return m_depthFormat; }

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

        static VkFormat findSupportedDepthFormat(VkPhysicalDevice gpu);

        void createImageViews();
        void createDepthResources();
        void destroyDepthResources();

        void createRenderPass();
        void createFramebuffers();

        void createColorMsaaResources();

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

        // MSAA color
        VkImage m_colorMsaaImage = VK_NULL_HANDLE;
        VkDeviceMemory m_colorMsaaMemory = VK_NULL_HANDLE;
        VkImageView m_colorMsaaView = VK_NULL_HANDLE;

        // depth
        VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
        VkImage m_depthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
        VkImageView m_depthView = VK_NULL_HANDLE;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_framebuffers;

        VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    };

    // ---------------- CommandPool ----------------
    class CommandPool
    {
    public:
        CommandPool() = default;
        ~CommandPool();

        CommandPool(const CommandPool &) = delete;
        CommandPool &operator=(const CommandPool &) = delete;

        void create(VkDevice device, uint32_t queueFamilyIndex);
        void destroy();

        void reset();
        void allocate(uint32_t count);

        VkCommandBuffer at(size_t i) const { return m_cmdBufs[i]; }
        size_t size() const { return m_cmdBufs.size(); }
        VkCommandPool handle() const { return m_pool; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_cmdBufs;
    };

    // ---------------- FrameSync ----------------
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
        VkFence inFlightFence() const { return m_inFlight[m_frameIndex]; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSemaphore m_imageAvailable[MAX_FRAMES]{};
        VkFence m_inFlight[MAX_FRAMES]{};
        uint32_t m_frameIndex = 0;
    };

    // ---------------- VulkanContext ----------------
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

        // getters used by GraphicsAPI
        VkDevice GetDevice() const { return m_device; }
        VkPhysicalDevice GetGPU() const { return m_gpu; }
        VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
        VkCommandPool GetCommandPool() const { return m_cmdPool.handle(); }

        VkRenderPass GetRenderPass() const { return m_swapchain.renderPass(); }
        VkExtent2D GetExtent() const { return m_swapchain.extent(); }

        VkDescriptorSetLayout GetCameraSetLayout() const { return m_cameraSetLayout; }
        VkDescriptorSet CurrentCameraSet() const { return m_cameraSets[m_sync.frameIndex()]; }

        VkDescriptorSetLayout GetTextureSetLayout() const { return m_textureSetLayout; }
        VkDescriptorSet CreateTextureSet(VkImageView view, VkSampler sampler);

        VkSampleCountFlagBits GetMsaaSamples() const { return m_msaaSamples; }

    private:
        struct QueueFamilies
        {
            std::optional<uint32_t> graphics;
            std::optional<uint32_t> present;
            bool complete() const { return graphics.has_value() && present.has_value(); }
        };

        struct alignas(16) CameraUBO
        {
            glm::mat4 view{1.0f};
            glm::mat4 proj{1.0f};
        };

    private:
        void createInstance(SDL_Window *window);
        void setupDebugMessenger();
        void createSurface(SDL_Window *window);
        void pickPhysicalDevice();
        void createDevice();

        void recordCommandBuffer(uint32_t imageIndex, SDL_Window *window);
        void recreateSwapchain(SDL_Window *window);

        static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface);
        static bool hasDeviceExtension(VkPhysicalDevice gpu, const char *extName);
        static bool checkValidationLayerSupport();

        void createPerImageSync();
        void destroyPerImageSync();

        void createCameraUBO();
        void destroyCameraUBO();

        void buildCameraData(SDL_Window *window, CameraData &out) const;
        void updateCameraUBO(const CameraData &cameraData);

        void createTextureDescriptors();
        void destroyTextureDescriptors();

    private:
#ifndef NDEBUG
        static constexpr bool kEnableValidation = true;
#else
        static constexpr bool kEnableValidation = false;
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

        // per swapchain image sync
        std::vector<VkSemaphore> m_renderFinishedPerImage;
        std::vector<VkFence> m_imagesInFlight;

        // shader programs (strong refs so we can Destroy before device)
        std::vector<std::shared_ptr<ShaderProgram>> m_programs;

        // Camera UBO: set=0 binding=0
        VkDescriptorSetLayout m_cameraSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_descPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_cameraSets;

        std::vector<VkBuffer> m_cameraBuffers;
        std::vector<VkDeviceMemory> m_cameraMemories;
        std::vector<void *> m_cameraMapped;

        VkDescriptorSetLayout m_textureSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_textureDescPool = VK_NULL_HANDLE;

        VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    };

}
