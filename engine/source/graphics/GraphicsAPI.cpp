#include "graphics/GraphicsAPI.h"

#include "graphics/ShaderProgram.h"
#include "Engine.h"
#include "vk/VulkanContext.h"
#include "render/Material.h"
#include "render/Mesh.h"

namespace eng
{

    std::shared_ptr<ShaderProgram> GraphicsAPI::CreateShaderProgram(const std::string &vertSpv,
                                                                    const std::string &fragSpv,
                                                                    const VertexLayout &layout)
    {
        auto &vk = Engine::GetInstance().GetVulkanContext();
        auto sp = std::make_shared<ShaderProgram>();
        sp->Create(vk.GetDevice(), vk.GetRenderPass(), vk.GetExtent(), layout, vertSpv, fragSpv);

        vk.RegisterShaderProgram(sp); // чтобы пересоздавать на resize (см. ниже)
        return sp;
    }

    void GraphicsAPI::SetClearColor(float r, float g, float b, float a)
    {
        m_clearColor[0] = r;
        m_clearColor[1] = g;
        m_clearColor[2] = b;
        m_clearColor[3] = a;
    }

    void GraphicsAPI::BindShaderProgram(ShaderProgram *shaderProgram)
    {
        if (shaderProgram)
            shaderProgram->Bind();
    }

    void GraphicsAPI::BindMaterial(Material *material)
    {
        if (material)
            material->Bind();
    }

    void GraphicsAPI::BindMesh(Mesh *mesh)
    {
        if (mesh)
            mesh->Bind();
    }

    void GraphicsAPI::DrawMesh(Mesh *mesh)
    {
        if (mesh)
            mesh->Draw();
    }

    uint32_t GraphicsAPI::FindMemoryType(VkPhysicalDevice gpu, uint32_t typeBits, VkMemoryPropertyFlags props)
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

    void GraphicsAPI::CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkBuffer &outBuf, VkDeviceMemory &outMem)
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bi, nullptr, &outBuf) != VK_SUCCESS)
            throw std::runtime_error("vkCreateBuffer failed");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, outBuf, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(gpu, req.memoryTypeBits, memProps);

        if (vkAllocateMemory(device, &ai, nullptr, &outMem) != VK_SUCCESS)
            throw std::runtime_error("vkAllocateMemory failed");

        vkBindBufferMemory(device, outBuf, outMem, 0);
    }

    void GraphicsAPI::CopyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool, VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool = pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        vkAllocateCommandBuffers(device, &cai, &cmd);

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        VkBufferCopy copy{};
        copy.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    VkBuffer GraphicsAPI::CreateVertexBuffer(const std::vector<float> &vertices)
    {
        if (vertices.empty())
            return VK_NULL_HANDLE;

        auto &vk = Engine::GetInstance().GetVulkanContext();
        VkDevice device = vk.GetDevice();
        VkPhysicalDevice gpu = vk.GetGPU();

        VkDeviceSize size = sizeof(float) * vertices.size();

        // staging
        VkBuffer stagingBuf{};
        VkDeviceMemory stagingMem{};
        CreateBuffer(gpu, device, size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);

        void *mapped = nullptr;
        vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
        std::memcpy(mapped, vertices.data(), (size_t)size);
        vkUnmapMemory(device, stagingMem);

        // device local
        VkBuffer vb{};
        VkDeviceMemory vbMem{};
        CreateBuffer(gpu, device, size,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     vb, vbMem);

        CopyBuffer(device, vk.GetGraphicsQueue(), vk.GetCommandPool(), stagingBuf, vb, size);

        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);

        m_ownedBuffers.push_back({vb, vbMem});
        return vb;
    }

    VkBuffer GraphicsAPI::CreateIndexBuffer(const std::vector<uint32_t> &indices)
    {
        if (indices.empty())
            return VK_NULL_HANDLE;

        auto &vk = Engine::GetInstance().GetVulkanContext();
        VkDevice device = vk.GetDevice();
        VkPhysicalDevice gpu = vk.GetGPU();

        VkDeviceSize size = sizeof(uint32_t) * indices.size();

        // staging
        VkBuffer stagingBuf{};
        VkDeviceMemory stagingMem{};
        CreateBuffer(gpu, device, size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);

        void *mapped = nullptr;
        vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
        std::memcpy(mapped, indices.data(), (size_t)size);
        vkUnmapMemory(device, stagingMem);

        // device local
        VkBuffer ib{};
        VkDeviceMemory ibMem{};
        CreateBuffer(gpu, device, size,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     ib, ibMem);

        CopyBuffer(device, vk.GetGraphicsQueue(), vk.GetCommandPool(), stagingBuf, ib, size);

        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);

        m_ownedBuffers.push_back({ib, ibMem});
        return ib;
    }

    void GraphicsAPI::DestroyBuffers()
    {
        auto &vk = Engine::GetInstance().GetVulkanContext();
        VkDevice device = vk.GetDevice();

        for (auto &r : m_ownedBuffers)
        {
            if (r.buffer)
                vkDestroyBuffer(device, r.buffer, nullptr);
            if (r.memory)
                vkFreeMemory(device, r.memory, nullptr);
            r.buffer = VK_NULL_HANDLE;
            r.memory = VK_NULL_HANDLE;
        }
        m_ownedBuffers.clear();
    }
}
