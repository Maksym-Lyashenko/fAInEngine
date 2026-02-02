#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "graphics/VertexLayout.h"

namespace eng
{

    class ShaderProgram
    {
    public:
        ShaderProgram() = default;
        ~ShaderProgram();

        ShaderProgram(const ShaderProgram &) = delete;
        ShaderProgram &operator=(const ShaderProgram &) = delete;

        void Create(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                    const VertexLayout &layout,
                    const std::string &vertSpv, const std::string &fragSpv,
                    VkDescriptorSetLayout cameraSetLayout, VkDescriptorSetLayout textureSetLayout);

        // swapchain recreate
        void Recreate(VkRenderPass rp, VkExtent2D extent);

        void Destroy();

        void Bind();
        void SetUniform(const std::string &name, float v);
        void SetUniform(const std::string &name, float v0, float v1);
        void SetUniform(const std::string &name, const glm::vec3 &v);
        void SetUniform(const std::string &name, const glm::vec4 &v);
        void SetUniform(const std::string &name, const glm::mat4 &m);

        VkPipelineLayout GetLayout() const { return m_layout; }

    private:
        // Must match GLSL push_constant block exactly
        struct PushData
        {
            glm::mat4 u_model = glm::mat4(1.0f);                // 64 bytes
            glm::vec4 u_color = glm::vec4(1.f, 1.f, 1.f, 1.f);  // 16 bytes
            glm::vec4 u_params = glm::vec4(0.f, 0.f, 1.f, 0.f); // x=time, y=value, z=strength, w=unused

            glm::vec4 u_lightPos = glm::vec4(0.f, 0.f, 0.f, 1.f);
            glm::vec4 u_lightColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
            glm::vec4 u_cameraPos = glm::vec4(0.f, 0.f, 0.f, 1.f);
        };

    private:
        VkShaderModule loadModule(const std::string &spvPath);
        void createPipelineLayoutIfNeeded();
        void recreatePipelineInternal(); // uses m_renderPass/m_extent
        void pushConstantsNow();         // vkCmdPushConstants if cmd is active

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        VertexLayout m_vlayout{};
        std::string m_vertPath, m_fragPath;

        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;

        PushData m_pc{};

        VkDescriptorSetLayout m_cameraSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureSetLayout = VK_NULL_HANDLE;
    };

}
