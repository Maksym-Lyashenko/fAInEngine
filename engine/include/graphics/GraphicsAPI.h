#pragma once

#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace eng
{
    class ShaderProgram;
    class Material;
    class Mesh;
    struct VertexLayout;

    struct BufferResource
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    class GraphicsAPI
    {
    public:
        std::shared_ptr<ShaderProgram> CreateShaderProgram(const std::string &vertSpv,
                                                           const std::string &fragSpv,
                                                           const VertexLayout &layout);

        void SetClearColor(float r, float g, float b, float a);

        void Begin(VkCommandBuffer cmd) { m_cmd = cmd; }
        void End() { m_cmd = VK_NULL_HANDLE; }

        VkCommandBuffer GetCmd() const { return m_cmd; }

        void SetCurrentPipelineLayout(VkPipelineLayout l) { m_currentLayout = l; }
        VkPipelineLayout GetCurrentPipelineLayout() const { return m_currentLayout; }

        void BindShaderProgram(ShaderProgram *shaderProgram);
        void BindMaterial(Material *material);
        void BindMesh(Mesh *mesh);
        void DrawMesh(Mesh *mesh);

        VkBuffer CreateVertexBuffer(const std::vector<float> &vertices);
        VkBuffer CreateIndexBuffer(const std::vector<uint32_t> &indices);
        void DestroyBuffers();

        const float *ClearColor() const { return m_clearColor; }

        void SetCurrentDescriptorSet(VkDescriptorSet set) { m_currenDescriptortSet = set; }
        VkDescriptorSet GetCurrentDescriptorSet() const { return m_currenDescriptortSet; }

    private:
        VkCommandBuffer m_cmd = VK_NULL_HANDLE;
        VkPipelineLayout m_currentLayout = VK_NULL_HANDLE;
        float m_clearColor[4] = {0.05f, 0.05f, 0.08f, 1.0f};

        std::vector<BufferResource> m_ownedBuffers;

        VkDescriptorSet m_currenDescriptortSet = VK_NULL_HANDLE;
    };

}
