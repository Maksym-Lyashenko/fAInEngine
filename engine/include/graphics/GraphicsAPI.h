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

        const std::shared_ptr<ShaderProgram> &GetDefaultShaderProgram();

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

        void SetCurrentCameraSet(VkDescriptorSet set) { m_cameraSet = set; }
        VkDescriptorSet GetCurrentCameraSet() const { return m_cameraSet; }

        void SetCurrentTextureSet(VkDescriptorSet set) { m_textureSet = set; }
        VkDescriptorSet GetCurrentTextureSet() const { return m_textureSet; }

    private:
        VkCommandBuffer m_cmd = VK_NULL_HANDLE;
        VkPipelineLayout m_currentLayout = VK_NULL_HANDLE;
        float m_clearColor[4] = {0.05f, 0.05f, 0.08f, 1.0f};

        std::vector<BufferResource> m_ownedBuffers;

        VkDescriptorSet m_cameraSet = VK_NULL_HANDLE;
        VkDescriptorSet m_textureSet = VK_NULL_HANDLE;

        std::shared_ptr<ShaderProgram> m_defaultShaderProgram;
    };

}
