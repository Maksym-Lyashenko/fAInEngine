#include "vk/VulkanContext.h"

#include <SDL3/SDL_vulkan.h>

#include "Engine.h"
#include "graphics/ShaderProgram.h"
#include "render/RenderQueue.h" // CameraData
#include "scene/GameObject.h"
#include "scene/components/CameraComponent.h"
#include "vk/VkHelpers.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace eng
{
    // ---- Debug Utils (VK_EXT_debug_utils) ----
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void * /*userData*/)
    {
        const char *sev =
            (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN"
                                                                               : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)      ? "INFO"
                                                                                                                                                : "VERBOSE";

        std::cerr << "[VK][" << sev << "] " << callbackData->pMessage << "\n";
        return VK_FALSE;
    }

    static VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
        const VkAllocationCallbacks *allocator,
        VkDebugUtilsMessengerEXT *messenger)
    {
        auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (!fn)
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        return fn(instance, createInfo, allocator, messenger);
    }

    static void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks *allocator)
    {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn)
            fn(instance, messenger, allocator);
    }

    // ---------------- Swapchain ----------------

    Swapchain::~Swapchain()
    {
        destroy();
    }

    Swapchain::Support Swapchain::querySupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        Support s{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &s.caps);

        uint32_t count = 0;

        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
        s.formats.resize(count);
        if (count)
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, s.formats.data());

        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
        s.presentModes.resize(count);
        if (count)
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, s.presentModes.data());

        return s;
    }

    VkSurfaceFormatKHR Swapchain::chooseFormat(const std::vector<VkSurfaceFormatKHR> &formats)
    {
        for (auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        }
        return formats.front();
    }

    VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR> &modes)
    {
        for (auto m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
                return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR &caps, SDL_Window *window)
    {
        if (caps.currentExtent.width != 0xFFFFFFFF)
        {
            if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0)
                return {0, 0};
            return caps.currentExtent;
        }

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w == 0 || h == 0)
            return {0, 0};

        VkExtent2D e{};
        e.width = (uint32_t)std::clamp(w, (int)caps.minImageExtent.width, (int)caps.maxImageExtent.width);
        e.height = (uint32_t)std::clamp(h, (int)caps.minImageExtent.height, (int)caps.maxImageExtent.height);
        return e;
    }

    VkFormat Swapchain::findSupportedDepthFormat(VkPhysicalDevice gpu)
    {
        const VkFormat candidates[] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT};

        for (VkFormat f : candidates)
        {
            VkFormatProperties p{};
            vkGetPhysicalDeviceFormatProperties(gpu, f, &p);
            if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                return f;
        }

        throw std::runtime_error("No supported depth format");
    }

    static VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect)
    {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image = image;
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = format;
        iv.subresourceRange.aspectMask = aspect;
        iv.subresourceRange.baseMipLevel = 0;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        vkutil::vkCheck(vkCreateImageView(device, &iv, nullptr, &view), "vkCreateImageView failed");
        return view;
    }

    static void CreateImage(VkPhysicalDevice gpu, VkDevice device,
                            uint32_t w, uint32_t h,
                            VkFormat format,
                            VkImageUsageFlags usage,
                            VkSampleCountFlagBits samples,
                            VkImage &outImage, VkDeviceMemory &outMem)
    {
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.extent = {w, h, 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.format = format;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci.usage = usage;
        ci.samples = samples;
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

    void Swapchain::createDepthResources()
    {
        destroyDepthResources();

        m_depthFormat = findSupportedDepthFormat(m_gpu);

        CreateImage(m_gpu, m_device,
                    m_extent.width, m_extent.height,
                    m_depthFormat,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    m_msaaSamples,
                    m_depthImage,
                    m_depthMemory);

        m_depthView = CreateImageView(m_device, m_depthImage, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void Swapchain::destroyDepthResources()
    {
        if (!m_device)
            return;

        if (m_depthView)
        {
            vkDestroyImageView(m_device, m_depthView, nullptr);
            m_depthView = VK_NULL_HANDLE;
        }
        if (m_depthImage)
        {
            vkDestroyImage(m_device, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }
        if (m_depthMemory)
        {
            vkFreeMemory(m_device, m_depthMemory, nullptr);
            m_depthMemory = VK_NULL_HANDLE;
        }
        m_depthFormat = VK_FORMAT_UNDEFINED;
    }

    void Swapchain::create(VkPhysicalDevice gpu,
                           VkDevice device,
                           VkSurfaceKHR surface,
                           SDL_Window *window,
                           uint32_t qGraphics,
                           uint32_t qPresent,
                           VkSampleCountFlagBits msaaSamples)
    {
        m_gpu = gpu;
        m_device = device;
        m_surface = surface;
        m_qGraphics = qGraphics;
        m_qPresent = qPresent;
        m_msaaSamples = msaaSamples;

        auto supp = querySupport(m_gpu, m_surface);
        if (supp.formats.empty() || supp.presentModes.empty())
            throw std::runtime_error("Swapchain support missing");

        auto fmt = chooseFormat(supp.formats);
        auto pm = choosePresentMode(supp.presentModes);
        auto ext = chooseExtent(supp.caps, window);

        if (ext.width == 0 || ext.height == 0)
            throw std::runtime_error("Swapchain extent is 0 (window minimized)");

        uint32_t imageCount = supp.caps.minImageCount + 1;
        if (supp.caps.maxImageCount > 0 && imageCount > supp.caps.maxImageCount)
            imageCount = supp.caps.maxImageCount;

        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        ci.surface = m_surface;
        ci.minImageCount = imageCount;
        ci.imageFormat = fmt.format;
        ci.imageColorSpace = fmt.colorSpace;
        ci.imageExtent = ext;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ci.preTransform = supp.caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = pm;
        ci.clipped = VK_TRUE;

        uint32_t qfi[] = {m_qGraphics, m_qPresent};
        if (m_qGraphics != m_qPresent)
        {
            ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices = qfi;
        }
        else
        {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        vkutil::vkCheck(vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain), "vkCreateSwapchainKHR failed");

        m_format = fmt.format;
        m_extent = ext;

        uint32_t count = 0;
        vkutil::vkCheck(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr), "vkGetSwapchainImagesKHR failed");
        m_images.resize(count);
        vkutil::vkCheck(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_images.data()), "vkGetSwapchainImagesKHR failed(2)");

        createImageViews();
        createColorMsaaResources();
        createDepthResources();
        createRenderPass();
        createFramebuffers();
    }

    void Swapchain::createImageViews()
    {
        m_views.resize(m_images.size());

        for (size_t i = 0; i < m_images.size(); ++i)
        {
            VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            iv.image = m_images[i];
            iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv.format = m_format;
            iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            iv.subresourceRange.baseMipLevel = 0;
            iv.subresourceRange.levelCount = 1;
            iv.subresourceRange.baseArrayLayer = 0;
            iv.subresourceRange.layerCount = 1;

            vkutil::vkCheck(vkCreateImageView(m_device, &iv, nullptr, &m_views[i]), "vkCreateImageView failed");
        }
    }
    void Swapchain::createRenderPass()
    {
        VkAttachmentDescription colorMsaa{};
        colorMsaa.format = m_format;
        colorMsaa.samples = m_msaaSamples;
        colorMsaa.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorMsaa.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorMsaa.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorMsaa.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorMsaa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorMsaa.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = m_depthFormat;
        depth.samples = m_msaaSamples;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorResolve{};
        colorResolve.format = m_format;
        colorResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference resolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colorRef;
        sub.pResolveAttachments = &resolveRef;
        sub.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkAttachmentDescription attachments[3] = {colorMsaa, depth, colorResolve};

        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount = 3;
        rp.pAttachments = attachments;
        rp.subpassCount = 1;
        rp.pSubpasses = &sub;
        rp.dependencyCount = 1;
        rp.pDependencies = &dep;

        vkutil::vkCheck(vkCreateRenderPass(m_device, &rp, nullptr, &m_renderPass), "vkCreateRenderPass failed");
    }

    void Swapchain::createFramebuffers()
    {
        m_framebuffers.resize(m_views.size());

        for (size_t i = 0; i < m_views.size(); ++i)
        {
            VkImageView attachments[] = {m_colorMsaaView, m_depthView, m_views[i]};

            VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbi.renderPass = m_renderPass;
            fbi.attachmentCount = 3;
            fbi.pAttachments = attachments;
            fbi.width = m_extent.width;
            fbi.height = m_extent.height;
            fbi.layers = 1;

            vkutil::vkCheck(vkCreateFramebuffer(m_device, &fbi, nullptr, &m_framebuffers[i]), "vkCreateFramebuffer failed");
        }
    }

    void Swapchain::createColorMsaaResources()
    {
        // destroy old
        if (m_colorMsaaView)
            vkDestroyImageView(m_device, m_colorMsaaView, nullptr);
        if (m_colorMsaaImage)
            vkDestroyImage(m_device, m_colorMsaaImage, nullptr);
        if (m_colorMsaaMemory)
            vkFreeMemory(m_device, m_colorMsaaMemory, nullptr);

        m_colorMsaaView = VK_NULL_HANDLE;
        m_colorMsaaImage = VK_NULL_HANDLE;
        m_colorMsaaMemory = VK_NULL_HANDLE;

        // create multisampled color image
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.extent = {m_extent.width, m_extent.height, 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.format = m_format;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ci.samples = m_msaaSamples;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkutil::vkCheck(vkCreateImage(m_device, &ci, nullptr, &m_colorMsaaImage), "vkCreateImage MSAA color failed");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(m_device, m_colorMsaaImage, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = vkutil::FindMemoryType(m_gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkutil::vkCheck(vkAllocateMemory(m_device, &ai, nullptr, &m_colorMsaaMemory), "vkAllocateMemory MSAA color failed");
        vkutil::vkCheck(vkBindImageMemory(m_device, m_colorMsaaImage, m_colorMsaaMemory, 0), "vkBindImageMemory MSAA color failed");

        // view
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image = m_colorMsaaImage;
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = m_format;
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel = 0;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount = 1;

        vkutil::vkCheck(vkCreateImageView(m_device, &iv, nullptr, &m_colorMsaaView), "vkCreateImageView MSAA color failed");
    }

    void Swapchain::destroy()
    {
        if (!m_device)
            return;

        for (auto fb : m_framebuffers)
            vkDestroyFramebuffer(m_device, fb, nullptr);
        m_framebuffers.clear();

        if (m_renderPass)
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;

        for (auto v : m_views)
            vkDestroyImageView(m_device, v, nullptr);
        m_views.clear();

        if (m_colorMsaaView)
            vkDestroyImageView(m_device, m_colorMsaaView, nullptr);
        if (m_colorMsaaImage)
            vkDestroyImage(m_device, m_colorMsaaImage, nullptr);
        if (m_colorMsaaMemory)
            vkFreeMemory(m_device, m_colorMsaaMemory, nullptr);
        m_colorMsaaView = VK_NULL_HANDLE;
        m_colorMsaaImage = VK_NULL_HANDLE;
        m_colorMsaaMemory = VK_NULL_HANDLE;

        destroyDepthResources();

        if (m_swapchain)
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;

        m_images.clear();
    }

    void Swapchain::recreate(SDL_Window *window)
    {
        destroy();
        create(m_gpu, m_device, m_surface, window, m_qGraphics, m_qPresent, m_msaaSamples);
    }

    bool Swapchain::hasAdequateSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        auto s = querySupport(gpu, surface);
        return !s.formats.empty() && !s.presentModes.empty();
    }

    // ---------------- CommandPool ----------------

    CommandPool::~CommandPool()
    {
        destroy();
    }

    void CommandPool::create(VkDevice device, uint32_t queueFamilyIndex)
    {
        m_device = device;

        VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.queueFamilyIndex = queueFamilyIndex;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkutil::vkCheck(vkCreateCommandPool(m_device, &ci, nullptr, &m_pool), "vkCreateCommandPool failed");
    }

    void CommandPool::destroy()
    {
        if (!m_device)
            return;

        if (m_pool)
        {
            vkDestroyCommandPool(m_device, m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
        m_cmdBufs.clear();
    }

    void CommandPool::reset()
    {
        vkutil::vkCheck(vkResetCommandPool(m_device, m_pool, 0), "vkResetCommandPool failed");
        m_cmdBufs.clear();
    }

    void CommandPool::allocate(uint32_t count)
    {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = m_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = count;

        m_cmdBufs.resize(count);
        vkutil::vkCheck(vkAllocateCommandBuffers(m_device, &ai, m_cmdBufs.data()), "vkAllocateCommandBuffers failed");
    }

    // ---------------- FrameSync ----------------

    FrameSync::~FrameSync()
    {
        destroy();
    }

    void FrameSync::create(VkDevice device)
    {
        m_device = device;

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES; ++i)
        {
            vkutil::vkCheck(vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable[i]), "vkCreateSemaphore imageAvailable failed");
            vkutil::vkCheck(vkCreateFence(m_device, &fci, nullptr, &m_inFlight[i]), "vkCreateFence failed");
        }
    }

    void FrameSync::destroy()
    {
        if (!m_device)
            return;

        for (int i = 0; i < MAX_FRAMES; ++i)
        {
            if (m_imageAvailable[i])
                vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
            if (m_inFlight[i])
                vkDestroyFence(m_device, m_inFlight[i], nullptr);
            m_imageAvailable[i] = VK_NULL_HANDLE;
            m_inFlight[i] = VK_NULL_HANDLE;
        }
    }

    // ---------------- VulkanContext ----------------

    VulkanContext::~VulkanContext()
    {
        if (m_device)
            vkDeviceWaitIdle(m_device);

        // Destroy shader programs (pipelines/layouts) BEFORE device
        for (auto &sp : m_programs)
            if (sp)
                sp->Destroy();
        m_programs.clear();

        destroyCameraUBO();
        destroyPerImageSync();
        destroyTextureDescriptors();

        m_sync.destroy();
        m_cmdPool.destroy();
        m_swapchain.destroy();

        if (m_surface)
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;

        if (m_device)
            vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;

        if (m_debugMessenger)
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;

        if (m_instance)
            vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    void VulkanContext::waitIdle()
    {
        if (m_device)
            vkDeviceWaitIdle(m_device);
    }

    void VulkanContext::RegisterShaderProgram(const std::shared_ptr<ShaderProgram> &sp)
    {
        m_programs.push_back(sp);
    }

    void VulkanContext::RecreateAllPrograms()
    {
        for (auto &sp : m_programs)
            if (sp)
                sp->Recreate(GetRenderPass(), GetExtent());
    }

    bool VulkanContext::checkValidationLayerSupport()
    {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        vkEnumerateInstanceLayerProperties(&count, props.data());

        const char *layerName = "VK_LAYER_KHRONOS_validation";
        for (auto &p : props)
            if (std::strcmp(p.layerName, layerName) == 0)
                return true;

        return false;
    }

    void VulkanContext::createPerImageSync()
    {
        destroyPerImageSync();

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        const size_t n = m_swapchain.imageCount();
        m_renderFinishedPerImage.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            vkutil::vkCheck(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinishedPerImage[i]),
                            "vkCreateSemaphore renderFinishedPerImage failed");
        }

        m_imagesInFlight.assign(n, VK_NULL_HANDLE);
    }

    void VulkanContext::destroyPerImageSync()
    {
        if (!m_device)
            return;

        for (auto s : m_renderFinishedPerImage)
            if (s)
                vkDestroySemaphore(m_device, s, nullptr);

        m_renderFinishedPerImage.clear();
        m_imagesInFlight.clear();
    }

    void VulkanContext::createCameraUBO()
    {
        // set=0 binding=0 UBO
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount = 1;
        li.pBindings = &b;

        vkutil::vkCheck(vkCreateDescriptorSetLayout(m_device, &li, nullptr, &m_cameraSetLayout),
                        "vkCreateDescriptorSetLayout failed");

        VkDescriptorPoolSize ps{};
        ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = FrameSync::MAX_FRAMES;

        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.maxSets = FrameSync::MAX_FRAMES;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;

        vkutil::vkCheck(vkCreateDescriptorPool(m_device, &pi, nullptr, &m_descPool),
                        "vkCreateDescriptorPool failed");

        m_cameraBuffers.resize(FrameSync::MAX_FRAMES);
        m_cameraMemories.resize(FrameSync::MAX_FRAMES);
        m_cameraMapped.resize(FrameSync::MAX_FRAMES);

        VkDeviceSize size = sizeof(CameraUBO);

        for (int i = 0; i < FrameSync::MAX_FRAMES; ++i)
        {
            vkutil::CreateBuffer(m_gpu, m_device, size,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 m_cameraBuffers[i], m_cameraMemories[i]);

            vkutil::vkCheck(vkMapMemory(m_device, m_cameraMemories[i], 0, size, 0, &m_cameraMapped[i]),
                            "vkMapMemory camera UBO failed");
        }

        std::vector<VkDescriptorSetLayout> layouts(FrameSync::MAX_FRAMES, m_cameraSetLayout);
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = m_descPool;
        ai.descriptorSetCount = FrameSync::MAX_FRAMES;
        ai.pSetLayouts = layouts.data();

        m_cameraSets.resize(FrameSync::MAX_FRAMES);
        vkutil::vkCheck(vkAllocateDescriptorSets(m_device, &ai, m_cameraSets.data()),
                        "vkAllocateDescriptorSets failed");

        for (int i = 0; i < FrameSync::MAX_FRAMES; ++i)
        {
            VkDescriptorBufferInfo bi{};
            bi.buffer = m_cameraBuffers[i];
            bi.offset = 0;
            bi.range = sizeof(CameraUBO);

            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = m_cameraSets[i];
            w.dstBinding = 0;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &bi;

            vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
        }
    }

    void VulkanContext::destroyCameraUBO()
    {
        if (!m_device)
            return;

        for (int i = 0; i < (int)m_cameraMapped.size(); ++i)
        {
            if (m_cameraMapped[i])
            {
                vkUnmapMemory(m_device, m_cameraMemories[i]);
                m_cameraMapped[i] = nullptr;
            }
        }

        for (int i = 0; i < (int)m_cameraBuffers.size(); ++i)
        {
            if (m_cameraBuffers[i])
                vkDestroyBuffer(m_device, m_cameraBuffers[i], nullptr);
            if (m_cameraMemories[i])
                vkFreeMemory(m_device, m_cameraMemories[i], nullptr);
        }

        m_cameraBuffers.clear();
        m_cameraMemories.clear();
        m_cameraMapped.clear();
        m_cameraSets.clear();

        if (m_descPool)
            vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;

        if (m_cameraSetLayout)
            vkDestroyDescriptorSetLayout(m_device, m_cameraSetLayout, nullptr);
        m_cameraSetLayout = VK_NULL_HANDLE;
    }

    void VulkanContext::buildCameraData(SDL_Window *window, CameraData &out) const
    {
        out.viewMatrix = glm::mat4(1.0f);
        out.projectionMatrix = glm::mat4(1.0f);
        out.position = glm::vec3(0.f);

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w <= 0 || h <= 0)
            return;

        const float aspect = static_cast<float>(w) / static_cast<float>(h);

        auto *scene = Engine::GetInstance().GetScene();
        if (!scene)
            return;

        auto cameraObject = scene->GetMainCamera();
        if (!cameraObject)
            return;

        auto cameraComponent = cameraObject->GetComponent<CameraComponent>();
        if (!cameraComponent)
            return;

        out.viewMatrix = cameraComponent->GetViewMatrix();
        out.projectionMatrix = cameraComponent->GetProjectionMatrix(aspect);
        out.position = cameraObject->GetWordPosition();
    }

    void VulkanContext::updateCameraUBO(const CameraData &cameraData)
    {
        const uint32_t fi = m_sync.frameIndex();
        if (fi >= m_cameraMapped.size() || !m_cameraMapped[fi])
            return;

        CameraUBO ubo{};
        ubo.view = cameraData.viewMatrix;
        ubo.proj = cameraData.projectionMatrix;

        // Vulkan Y flip (если projection из glm::perspective OpenGL-style)
        ubo.proj[1][1] *= -1.0f;

        std::memcpy(m_cameraMapped[fi], &ubo, sizeof(CameraUBO));
    }

    void VulkanContext::createTextureDescriptors()
    {
        // set=1 binding=0 sampler2D
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount = 1;
        li.pBindings = &b;

        vkutil::vkCheck(vkCreateDescriptorSetLayout(m_device, &li, nullptr, &m_textureSetLayout),
                        "vkCreateDescriptorSetLayout (texture) failed");

        VkDescriptorPoolSize ps{};
        ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = 256; // enough for now

        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.maxSets = 256;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;

        vkutil::vkCheck(vkCreateDescriptorPool(m_device, &pi, nullptr, &m_textureDescPool),
                        "vkCreateDescriptorPool (texture) failed");
    }

    void VulkanContext::destroyTextureDescriptors()
    {
        if (!m_device)
            return;

        if (m_textureDescPool)
        {
            vkDestroyDescriptorPool(m_device, m_textureDescPool, nullptr);
            m_textureDescPool = VK_NULL_HANDLE;
        }
        if (m_textureSetLayout)
        {
            vkDestroyDescriptorSetLayout(m_device, m_textureSetLayout, nullptr);
            m_textureSetLayout = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSet VulkanContext::CreateTextureSet(VkImageView view, VkSampler sampler)
    {
        if (!m_textureDescPool || !m_textureSetLayout)
            throw std::runtime_error("Texture descriptor resources not created");

        VkDescriptorSet set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = m_textureDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_textureSetLayout;

        vkutil::vkCheck(vkAllocateDescriptorSets(m_device, &ai, &set), "vkAllocateDescriptorSets (texture) failed");

        VkDescriptorImageInfo ii{};
        ii.sampler = sampler;
        ii.imageView = view;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii;

        vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
        return set;
    }

    VulkanContext::QueueFamilies VulkanContext::findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        QueueFamilies out;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                out.graphics = i;

            VkBool32 canPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &canPresent);
            if (canPresent)
                out.present = i;

            if (out.complete())
                break;
        }
        return out;
    }

    bool VulkanContext::hasDeviceExtension(VkPhysicalDevice gpu, const char *extName)
    {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> exts(count);
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, exts.data());

        for (auto &e : exts)
            if (std::strcmp(e.extensionName, extName) == 0)
                return true;

        return false;
    }

    void VulkanContext::init(SDL_Window *window)
    {
        createInstance(window);
        setupDebugMessenger();
        createSurface(window);
        pickPhysicalDevice();
        createDevice();

        createCameraUBO();
        createTextureDescriptors();

        m_swapchain.create(m_gpu, m_device, m_surface, window, m_qGraphics, m_qPresent, m_msaaSamples);

        m_cmdPool.create(m_device, m_qGraphics);
        m_cmdPool.allocate((uint32_t)m_swapchain.imageCount());

        m_sync.create(m_device);
        createPerImageSync();
    }

    void VulkanContext::createInstance(SDL_Window *window)
    {
        Uint32 extCount = 0;
        const char *const *sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        std::vector<const char *> exts(sdlExts, sdlExts + extCount);

        const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

        VkDebugUtilsMessengerCreateInfoEXT dbgCI{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};

        if constexpr (kEnableValidation)
        {
            if (!checkValidationLayerSupport())
                throw std::runtime_error("VK_LAYER_KHRONOS_validation not found");

            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            dbgCI.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            dbgCI.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

            dbgCI.pfnUserCallback = DebugCallback;
        }

        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "fAInEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "fAInEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &appInfo;
        ci.enabledExtensionCount = (uint32_t)exts.size();
        ci.ppEnabledExtensionNames = exts.data();

        if constexpr (kEnableValidation)
        {
            ci.enabledLayerCount = 1;
            ci.ppEnabledLayerNames = layers;
            ci.pNext = &dbgCI;
        }

        vkutil::vkCheck(vkCreateInstance(&ci, nullptr, &m_instance), "vkCreateInstance failed");
    }

    void VulkanContext::setupDebugMessenger()
    {
        if constexpr (!kEnableValidation)
            return;

        VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        ci.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = DebugCallback;

        vkutil::vkCheck(CreateDebugUtilsMessengerEXT(m_instance, &ci, nullptr, &m_debugMessenger),
                        "CreateDebugUtilsMessengerEXT failed");
    }

    void VulkanContext::createSurface(SDL_Window *window)
    {
        if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface))
            throw std::runtime_error(SDL_GetError());
    }

    void VulkanContext::pickPhysicalDevice()
    {
        uint32_t count = 0;
        vkutil::vkCheck(vkEnumeratePhysicalDevices(m_instance, &count, nullptr), "vkEnumeratePhysicalDevices failed");
        if (count == 0)
            throw std::runtime_error("No Vulkan GPUs found");

        std::vector<VkPhysicalDevice> devs(count);
        vkutil::vkCheck(vkEnumeratePhysicalDevices(m_instance, &count, devs.data()), "vkEnumeratePhysicalDevices failed(2)");

        for (auto d : devs)
        {
            if (!hasDeviceExtension(d, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                continue;

            auto q = findQueueFamilies(d, m_surface);
            if (!q.complete())
                continue;

            if (!Swapchain::hasAdequateSupport(d, m_surface))
                continue;

            m_gpu = d;
            m_msaaSamples = vkutil::GetMaxUsableSampleCount(m_gpu);
            m_qGraphics = q.graphics.value();
            m_qPresent = q.present.value();
            return;
        }

        throw std::runtime_error("No suitable GPU found");
    }

    void VulkanContext::createDevice()
    {
        float prio = 1.0f;

        std::vector<uint32_t> unique = {m_qGraphics};
        if (m_qPresent != m_qGraphics)
            unique.push_back(m_qPresent);

        std::vector<VkDeviceQueueCreateInfo> qcis;
        qcis.reserve(unique.size());

        for (uint32_t qf : unique)
        {
            VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qci.queueFamilyIndex = qf;
            qci.queueCount = 1;
            qci.pQueuePriorities = &prio;
            qcis.push_back(qci);
        }

        const char *devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkPhysicalDeviceFeatures supported{};
        vkGetPhysicalDeviceFeatures(m_gpu, &supported);

        VkPhysicalDeviceFeatures enabled{};
        if (supported.samplerAnisotropy)
            enabled.samplerAnisotropy = VK_TRUE;
        if (supported.sampleRateShading)
            enabled.sampleRateShading = VK_TRUE;

        VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = (uint32_t)qcis.size();
        ci.pQueueCreateInfos = qcis.data();
        ci.enabledExtensionCount = 1;
        ci.ppEnabledExtensionNames = devExts;
        ci.pEnabledFeatures = &enabled;

        vkutil::vkCheck(vkCreateDevice(m_gpu, &ci, nullptr, &m_device), "vkCreateDevice failed");

        vkGetDeviceQueue(m_device, m_qGraphics, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_qPresent, 0, &m_presentQueue);
    }

    void VulkanContext::recordCommandBuffer(uint32_t imageIndex, SDL_Window *window)
    {
        VkCommandBuffer cb = m_cmdPool.at(imageIndex);

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkutil::vkCheck(vkBeginCommandBuffer(cb, &bi), "vkBeginCommandBuffer failed");

        const float *cc = Engine::GetInstance().GetGraphicsAPI().ClearColor();

        VkClearValue clears[2]{};
        clears[0].color.float32[0] = cc[0];
        clears[0].color.float32[1] = cc[1];
        clears[0].color.float32[2] = cc[2];
        clears[0].color.float32[3] = cc[3];
        clears[1].depthStencil.depth = 1.0f;
        clears[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rbi.renderPass = m_swapchain.renderPass();
        rbi.framebuffer = m_swapchain.framebuffer(imageIndex);
        rbi.renderArea.offset = {0, 0};
        rbi.renderArea.extent = m_swapchain.extent();
        rbi.clearValueCount = 2;
        rbi.pClearValues = clears;

        vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);

        CameraData cameraData{};
        buildCameraData(window, cameraData);
        updateCameraUBO(cameraData);

        auto &api = Engine::GetInstance().GetGraphicsAPI();
        api.Begin(cb);
        api.SetCurrentCameraSet(CurrentCameraSet());

        std::vector<LightData> lights;
        auto *scene = Engine::GetInstance().GetScene();
        lights = scene->CollectLights();

        auto &rq = Engine::GetInstance().GetRenderQueue();
        rq.Draw(api, cameraData, lights);

        api.End();

        vkCmdEndRenderPass(cb);

        vkutil::vkCheck(vkEndCommandBuffer(cb), "vkEndCommandBuffer failed");
    }

    void VulkanContext::recreateSwapchain(SDL_Window *window)
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);

        if (w == 0 || h == 0)
        {
            m_framebufferResized = true;
            return;
        }

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_gpu, m_surface, &caps);
        if (caps.currentExtent.width != 0xFFFFFFFF &&
            (caps.currentExtent.width == 0 || caps.currentExtent.height == 0))
        {
            m_framebufferResized = true;
            return;
        }

        vkDeviceWaitIdle(m_device);

        m_swapchain.recreate(window);
        RecreateAllPrograms();

        m_cmdPool.reset();
        m_cmdPool.allocate((uint32_t)m_swapchain.imageCount());

        createPerImageSync();
        m_framebufferResized = false;
    }

    void VulkanContext::drawFrame(SDL_Window *window, bool resized)
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w == 0 || h == 0)
            return;

        if (resized)
            m_framebufferResized = true;

        VkFence fence = m_sync.inFlightFence();
        vkutil::vkCheck(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences failed");

        uint32_t imageIndex = 0;
        VkResult acq = vkAcquireNextImageKHR(
            m_device,
            m_swapchain.handle(),
            UINT64_MAX,
            m_sync.imageAvailable(),
            VK_NULL_HANDLE,
            &imageIndex);

        if (acq == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain(window);
            return;
        }
        if (acq == VK_SUBOPTIMAL_KHR)
        {
            m_framebufferResized = true;
        }
        vkutil::vkCheck(acq, "vkAcquireNextImageKHR failed");

        if (imageIndex < m_imagesInFlight.size() && m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            VkFence imgFence = m_imagesInFlight[imageIndex];
            vkutil::vkCheck(vkWaitForFences(m_device, 1, &imgFence, VK_TRUE, UINT64_MAX), "vkWaitForFences (image) failed");
        }

        vkutil::vkCheck(vkResetFences(m_device, 1, &fence), "vkResetFences failed");
        m_imagesInFlight[imageIndex] = fence;

        vkutil::vkCheck(vkResetCommandBuffer(m_cmdPool.at(imageIndex), 0), "vkResetCommandBuffer failed");
        recordCommandBuffer(imageIndex, window);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSemaphore waitSem = m_sync.imageAvailable();
        VkSemaphore signalSem = m_renderFinishedPerImage[imageIndex];

        VkCommandBuffer cb = m_cmdPool.at(imageIndex);

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &waitSem;
        si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &signalSem;

        vkutil::vkCheck(vkQueueSubmit(m_graphicsQueue, 1, &si, fence), "vkQueueSubmit failed");

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &signalSem;

        VkSwapchainKHR sc = m_swapchain.handle();
        pi.swapchainCount = 1;
        pi.pSwapchains = &sc;
        pi.pImageIndices = &imageIndex;

        VkResult pres = vkQueuePresentKHR(m_presentQueue, &pi);

        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain(window);
        }
        else
        {
            vkutil::vkCheck(pres, "vkQueuePresentKHR failed");
        }

        m_sync.advance();
    }

}
