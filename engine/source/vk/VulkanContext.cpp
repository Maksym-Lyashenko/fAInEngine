#include "vk/VulkanContext.h"

#include <SDL3/SDL_vulkan.h>

#include <Engine.h>
#include "graphics/ShaderProgram.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <vector>

namespace eng
{

    static void vkCheck(VkResult r, const char *msg)
    {
        if (r != VK_SUCCESS)
            throw std::runtime_error(msg);
    }

    // ---- Debug Utils (VK_EXT_debug_utils) ----
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
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

    // ===================== Swapchain =====================

    Swapchain::~Swapchain() { destroy(); }

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
        return VK_PRESENT_MODE_FIFO_KHR; // guaranteed
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

    void Swapchain::create(VkPhysicalDevice gpu,
                           VkDevice device,
                           VkSurfaceKHR surface,
                           SDL_Window *window,
                           uint32_t qGraphics,
                           uint32_t qPresent)
    {
        m_gpu = gpu;
        m_device = device;
        m_surface = surface;
        m_qGraphics = qGraphics;
        m_qPresent = qPresent;

        auto supp = querySupport(m_gpu, m_surface);
        if (supp.formats.empty() || supp.presentModes.empty())
            throw std::runtime_error("Swapchain support missing");

        auto fmt = chooseFormat(supp.formats);
        auto pm = choosePresentMode(supp.presentModes);
        auto ext = chooseExtent(supp.caps, window);

        if (ext.width == 0 || ext.height == 0)
        {
            throw std::runtime_error("Swapchain extent is 0 (window minimized)");
        }

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

        vkCheck(vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain), "vkCreateSwapchainKHR failed");

        m_format = fmt.format;
        m_extent = ext;

        // Images
        uint32_t count = 0;
        vkCheck(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr), "vkGetSwapchainImagesKHR failed");
        m_images.resize(count);
        vkCheck(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_images.data()), "vkGetSwapchainImagesKHR failed(2)");

        createImageViews();
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

            vkCheck(vkCreateImageView(m_device, &iv, nullptr, &m_views[i]), "vkCreateImageView failed");
        }
    }

    void Swapchain::createRenderPass()
    {
        VkAttachmentDescription color{};
        color.format = m_format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount = 1;
        rp.pAttachments = &color;
        rp.subpassCount = 1;
        rp.pSubpasses = &sub;
        rp.dependencyCount = 1;
        rp.pDependencies = &dep;

        vkCheck(vkCreateRenderPass(m_device, &rp, nullptr, &m_renderPass), "vkCreateRenderPass failed");
    }

    void Swapchain::createFramebuffers()
    {
        m_framebuffers.resize(m_views.size());

        for (size_t i = 0; i < m_views.size(); ++i)
        {
            VkImageView attachments[] = {m_views[i]};

            VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbi.renderPass = m_renderPass;
            fbi.attachmentCount = 1;
            fbi.pAttachments = attachments;
            fbi.width = m_extent.width;
            fbi.height = m_extent.height;
            fbi.layers = 1;

            vkCheck(vkCreateFramebuffer(m_device, &fbi, nullptr, &m_framebuffers[i]), "vkCreateFramebuffer failed");
        }
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

        if (m_swapchain)
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;

        m_images.clear();
    }

    void Swapchain::recreate(SDL_Window *window)
    {
        destroy();
        create(m_gpu, m_device, m_surface, window, m_qGraphics, m_qPresent);
    }

    // ===================== CommandPool =====================

    CommandPool::~CommandPool() { destroy(); }

    void CommandPool::create(VkDevice device, uint32_t queueFamilyIndex)
    {
        m_device = device;

        VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.queueFamilyIndex = queueFamilyIndex;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCheck(vkCreateCommandPool(m_device, &ci, nullptr, &m_pool), "vkCreateCommandPool failed");
    }

    void CommandPool::destroy()
    {
        if (!m_device)
            return;

        if (m_pool)
        {
            // destroying pool frees command buffers too
            vkDestroyCommandPool(m_device, m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
        m_cmdBufs.clear();
    }

    void CommandPool::reset()
    {
        vkCheck(vkResetCommandPool(m_device, m_pool, 0), "vkResetCommandPool failed");
        m_cmdBufs.clear();
    }

    void CommandPool::allocate(uint32_t count)
    {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = m_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = count;

        m_cmdBufs.resize(count);
        vkCheck(vkAllocateCommandBuffers(m_device, &ai, m_cmdBufs.data()), "vkAllocateCommandBuffers failed");
    }

    // ===================== FrameSync =====================

    FrameSync::~FrameSync() { destroy(); }

    void FrameSync::create(VkDevice device)
    {
        m_device = device;

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES; ++i)
        {
            vkCheck(vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable[i]), "vkCreateSemaphore imageAvailable failed");
            vkCheck(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinished[i]), "vkCreateSemaphore renderFinished failed");
            vkCheck(vkCreateFence(m_device, &fci, nullptr, &m_inFlight[i]), "vkCreateFence failed");
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
            if (m_renderFinished[i])
                vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
            if (m_inFlight[i])
                vkDestroyFence(m_device, m_inFlight[i], nullptr);
            m_imageAvailable[i] = VK_NULL_HANDLE;
            m_renderFinished[i] = VK_NULL_HANDLE;
            m_inFlight[i] = VK_NULL_HANDLE;
        }
    }

    // ===================== VulkanContext =====================

    VulkanContext::~VulkanContext()
    {
        // Proper teardown order
        if (m_device)
            vkDeviceWaitIdle(m_device);

        for (auto &sp : m_programs)
        {
            if (sp)
                sp->Destroy();
        }
        m_programs.clear();

        Engine::GetInstance().GetGraphicsAPI().DestroyBuffers();

        m_sync.destroy();
        m_cmdPool.destroy();
        destroyPerImageSync();
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
        {
            if (sp)
                sp->Recreate(GetRenderPass(), GetExtent());
        }
    }

    bool VulkanContext::checkValidationLayerSupport()
    {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        vkEnumerateInstanceLayerProperties(&count, props.data());

        const char *layerName = "VK_LAYER_KHRONOS_validation";
        for (auto &p : props)
        {
            if (std::strcmp(p.layerName, layerName) == 0)
                return true;
        }
        return false;
    }

    void VulkanContext::createPerImageSync()
    {
        // destroy old if any
        destroyPerImageSync();

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        const size_t n = m_swapchain.imageCount();
        m_renderFinishedPerImage.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            vkCheck(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinishedPerImage[i]),
                    "vkCreateSemaphore renderFinishedPerImage failed");
        }

        // fences that guard each swapchain image usage
        m_imagesInFlight.assign(n, VK_NULL_HANDLE);
    }

    void VulkanContext::destroyPerImageSync()
    {
        if (!m_device)
            return;

        for (auto s : m_renderFinishedPerImage)
        {
            if (s)
                vkDestroySemaphore(m_device, s, nullptr);
        }
        m_renderFinishedPerImage.clear();
        m_imagesInFlight.clear();
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

        // swapchain depends on device + surface
        m_swapchain.create(m_gpu, m_device, m_surface, window, m_qGraphics, m_qPresent);

        // command pool depends on device + graphics queue family
        m_cmdPool.create(m_device, m_qGraphics);
        m_cmdPool.allocate((uint32_t)m_swapchain.imageCount());

        // sync depends on device
        m_sync.create(m_device);
        createPerImageSync();
    }

    void VulkanContext::createInstance(SDL_Window *window)
    {
        // Extensions from SDL
        Uint32 extCount = 0;
        const char *const *sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        std::vector<const char *> exts(sdlExts, sdlExts + extCount);

        const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

        if constexpr (kEnableValidation)
        {
            if (!checkValidationLayerSupport())
                throw std::runtime_error("Validation layer VK_LAYER_KHRONOS_validation not found (install Vulkan SDK?)");

            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

        VkDebugUtilsMessengerCreateInfoEXT dbgCI{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        if constexpr (kEnableValidation)
        {
            ci.enabledLayerCount = 1;
            ci.ppEnabledLayerNames = layers;

            dbgCI.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            dbgCI.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

            dbgCI.pfnUserCallback = DebugCallback;

            // so we catch instance creation messages too
            ci.pNext = &dbgCI;
        }

        vkCheck(vkCreateInstance(&ci, nullptr, &m_instance), "vkCreateInstance failed");
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

        vkCheck(CreateDebugUtilsMessengerEXT(m_instance, &ci, nullptr, &m_debugMessenger),
                "CreateDebugUtilsMessengerEXT failed");
    }

    void VulkanContext::createSurface(SDL_Window *window)
    {
        // SDL3 signature you already use:
        if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface))
            throw std::runtime_error(SDL_GetError());
    }

    bool Swapchain::hasAdequateSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        auto s = querySupport(gpu, surface);
        return !s.formats.empty() && !s.presentModes.empty();
    }

    void VulkanContext::pickPhysicalDevice()
    {
        uint32_t count = 0;
        vkCheck(vkEnumeratePhysicalDevices(m_instance, &count, nullptr), "vkEnumeratePhysicalDevices failed");
        if (count == 0)
            throw std::runtime_error("No Vulkan GPUs found");

        std::vector<VkPhysicalDevice> devs(count);
        vkCheck(vkEnumeratePhysicalDevices(m_instance, &count, devs.data()), "vkEnumeratePhysicalDevices failed(2)");

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

        VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = (uint32_t)qcis.size();
        ci.pQueueCreateInfos = qcis.data();
        ci.enabledExtensionCount = 1;
        ci.ppEnabledExtensionNames = devExts;

        vkCheck(vkCreateDevice(m_gpu, &ci, nullptr, &m_device), "vkCreateDevice failed");

        vkGetDeviceQueue(m_device, m_qGraphics, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_qPresent, 0, &m_presentQueue);
    }

    void VulkanContext::recordCommandBuffer(uint32_t imageIndex)
    {
        if (imageIndex >= m_cmdPool.size())
            throw std::runtime_error("Command buffer index out of range");

        VkCommandBuffer cb = m_cmdPool.at(imageIndex);

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkCheck(vkBeginCommandBuffer(cb, &bi), "vkBeginCommandBuffer failed");

        auto *cc = Engine::GetInstance().GetGraphicsAPI().ClearColor();

        VkClearValue clear{};
        clear.color.float32[0] = cc[0];
        clear.color.float32[1] = cc[1];
        clear.color.float32[2] = cc[2];
        clear.color.float32[3] = cc[3];

        VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rbi.renderPass = m_swapchain.renderPass();
        rbi.framebuffer = m_swapchain.framebuffer(imageIndex);
        rbi.renderArea.offset = {0, 0};
        rbi.renderArea.extent = m_swapchain.extent();
        rbi.clearValueCount = 1;
        rbi.pClearValues = &clear;

        vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);

        auto &api = Engine::GetInstance().GetGraphicsAPI();
        api.Begin(cb);
        auto &renderQueue = Engine::GetInstance().GetRenderQueue();
        renderQueue.Draw(api);
        api.End();

        vkCmdEndRenderPass(cb);

        vkCheck(vkEndCommandBuffer(cb), "vkEndCommandBuffer failed");
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

        // command buffers must match swapchain image count
        m_cmdPool.reset();
        m_cmdPool.allocate((uint32_t)m_swapchain.imageCount());

        createPerImageSync();

        m_framebufferResized = false;
    }

    void VulkanContext::drawFrame(SDL_Window *window, bool resized)
    {
        // optional: skip rendering if minimized
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w == 0 || h == 0)
            return;

        if (resized)
            m_framebufferResized = true;

        VkFence fence = m_sync.inFlightFence();
        vkCheck(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences failed");

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
        vkCheck(acq, "vkAcquireNextImageKHR failed");

        if (imageIndex < m_imagesInFlight.size() && m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            VkFence imgFence = m_imagesInFlight[imageIndex];
            vkCheck(vkWaitForFences(m_device, 1, &imgFence, VK_TRUE, UINT64_MAX), "vkWaitForFences (image) failed");
        }

        vkCheck(vkResetFences(m_device, 1, &fence), "vkResetFences failed");

        m_imagesInFlight[imageIndex] = fence;

        // record
        vkCheck(vkResetCommandBuffer(m_cmdPool.at(imageIndex), 0), "vkResetCommandBuffer failed");
        recordCommandBuffer(imageIndex);

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

        vkCheck(vkQueueSubmit(m_graphicsQueue, 1, &si, fence), "vkQueueSubmit failed");

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
            vkCheck(pres, "vkQueuePresentKHR failed");
        }

        m_sync.advance();
    }

}
