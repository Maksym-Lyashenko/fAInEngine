#include "graphics/GraphicsAPI.h"

#include "graphics/ShaderProgram.h"
#include "Engine.h"
#include "vk/VulkanContext.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "vk/VkHelpers.h"

namespace eng
{

    std::shared_ptr<ShaderProgram> GraphicsAPI::CreateShaderProgram(const std::string &vertSpv,
                                                                    const std::string &fragSpv,
                                                                    const VertexLayout &layout)
    {
        auto &vk = Engine::GetInstance().GetVulkanContext();
        auto sp = std::make_shared<ShaderProgram>();
        sp->Create(vk.GetDevice(), vk.GetRenderPass(), vk.GetExtent(), layout, vertSpv, fragSpv, vk.GetCameraSetLayout(), vk.GetTextureSetLayout());

        vk.RegisterShaderProgram(sp); // чтобы пересоздавать на resize (см. ниже)
        return sp;
    }

    const std::shared_ptr<ShaderProgram> &GraphicsAPI::GetDefaultShaderProgram()
    {
        if (!m_defaultShaderProgram)
        {
            eng::VertexLayout layout;
            // Position
            layout.elements.push_back({VertexElement::Position, 3, AttribType::Float32, 0});
            // Color
            layout.elements.push_back({VertexElement::Color, 3, AttribType::Float32, sizeof(float) * 3});
            // UV
            layout.elements.push_back({VertexElement::UV, 2, AttribType::Float32, sizeof(float) * 6});
            // Normals
            layout.elements.push_back({VertexElement::Normal, 3, AttribType::Float32, sizeof(float) * 8});

            layout.stride = sizeof(float) * 11;

            m_defaultShaderProgram = CreateShaderProgram(
                "shaders/vertex.spv",
                "shaders/fragment.spv",
                layout);
        }

        return m_defaultShaderProgram;
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
        vkutil::CreateBuffer(gpu, device, size,
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
        vkutil::CreateBuffer(gpu, device, size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             vb, vbMem);

        vkutil::CopyBuffer(device, vk.GetGraphicsQueue(), vk.GetCommandPool(), stagingBuf, vb, size);

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
        vkutil::CreateBuffer(gpu, device, size,
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
        vkutil::CreateBuffer(gpu, device, size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             ib, ibMem);

        vkutil::CopyBuffer(device, vk.GetGraphicsQueue(), vk.GetCommandPool(), stagingBuf, ib, size);

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
