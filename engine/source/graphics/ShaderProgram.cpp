#include "graphics/ShaderProgram.h"
#include "Engine.h"
#include "graphics/GraphicsAPI.h"
#include "vk/VkHelpers.h"

#include <fstream>
#include <vector>
#include <stdexcept>

namespace eng
{

    ShaderProgram::~ShaderProgram()
    {
        Destroy();
    }

    VkShaderModule ShaderProgram::loadModule(const std::string &spvPath)
    {
        auto &fs = eng::Engine::GetInstance().GetFileSystem();

        auto code = fs.LoadAssetSpirv(spvPath);
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = code.size() * sizeof(uint32_t);
        ci.pCode = code.data();

        VkShaderModule mod{};
        if (vkCreateShaderModule(m_device, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("vkCreateShaderModule failed");
        return mod;
    }

    void ShaderProgram::createPipelineLayoutIfNeeded()
    {
        if (m_layout)
            return;
        if (m_cameraSetLayout == VK_NULL_HANDLE || m_textureSetLayout == VK_NULL_HANDLE)
            throw std::runtime_error("SetLayout is null");

        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0;
        range.size = sizeof(PushData);

        VkDescriptorSetLayout setLayouts[] = {m_cameraSetLayout, m_textureSetLayout};

        VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        li.setLayoutCount = 2;
        li.pSetLayouts = setLayouts;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &range;

        vkutil::vkCheck(vkCreatePipelineLayout(m_device, &li, nullptr, &m_layout),
                        "vkCreatePipelineLayout failed");
    }

    void ShaderProgram::Create(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                               const VertexLayout &layout,
                               const std::string &vertSpv, const std::string &fragSpv,
                               VkDescriptorSetLayout cameraSetLayout, VkDescriptorSetLayout textureSetLayout)
    {
        m_device = device;
        m_renderPass = renderPass;
        m_extent = extent;
        m_vlayout = layout;
        m_vertPath = vertSpv;
        m_fragPath = fragSpv;

        m_cameraSetLayout = cameraSetLayout;
        m_textureSetLayout = textureSetLayout;

        createPipelineLayoutIfNeeded();
        recreatePipelineInternal();
    }

    void ShaderProgram::Recreate(VkRenderPass renderPass, VkExtent2D extent)
    {
        m_renderPass = renderPass;
        m_extent = extent;
        recreatePipelineInternal();
    }

    void ShaderProgram::recreatePipelineInternal()
    {
        if (m_pipeline)
        {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        VkShaderModule vert = loadModule(m_vertPath);
        VkShaderModule frag = loadModule(m_fragPath);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";

        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        // Vertex input from your VertexLayout
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = m_vlayout.stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attrs;
        attrs.reserve(m_vlayout.elements.size());
        for (const auto &e : m_vlayout.elements)
        {
            VkVertexInputAttributeDescription a{};
            a.location = e.index;
            a.binding = 0;
            a.offset = e.offset;
            a.format = ToVkFormat(e.type, e.size);
            attrs.push_back(a);
        }

        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &binding;
        vi.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
        vi.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;

        VkViewport vp{};
        vp.width = (float)m_extent.width;
        vp.height = (float)m_extent.height;
        vp.minDepth = 0.f;
        vp.maxDepth = 1.f;

        VkRect2D sc{};
        sc.extent = m_extent;

        VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vpState.viewportCount = 1;
        vpState.pViewports = &vp;
        vpState.scissorCount = 1;
        vpState.pScissors = &sc;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.f;

        auto samples = Engine::GetInstance().GetVulkanContext().GetMsaaSamples();

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = samples;

        // option (a bit better quality MSAA):
        ms.sampleShadingEnable = VK_TRUE;
        ms.minSampleShading = 0.25f;

        VkPipelineColorBlendAttachmentState cbAtt{};
        cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &cbAtt;

        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        gp.stageCount = 2;
        gp.pStages = stages;
        gp.pVertexInputState = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vpState;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pColorBlendState = &cb;
        gp.layout = m_layout;
        gp.renderPass = m_renderPass;
        gp.pDepthStencilState = &ds;
        gp.subpass = 0;

        vkutil::vkCheck(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline),
                        "vkCreateGraphicsPipelines failed");

        vkDestroyShaderModule(m_device, vert, nullptr);
        vkDestroyShaderModule(m_device, frag, nullptr);
    }

    void ShaderProgram::Destroy()
    {
        if (m_device)
        {
            if (m_pipeline)
                vkDestroyPipeline(m_device, m_pipeline, nullptr);
            if (m_layout)
                vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        }
        m_pipeline = VK_NULL_HANDLE;
        m_layout = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
    }

    void ShaderProgram::Bind()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();
        if (cmd == VK_NULL_HANDLE)
            return; // Bind called outside recording

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        VkDescriptorSet sets[2] = {
            api.GetCurrentCameraSet(),
            api.GetCurrentTextureSet()

        };
        if (sets[0] != VK_NULL_HANDLE && sets[1] != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_layout, 0, 2, sets, 0, nullptr);
        }
        api.SetCurrentPipelineLayout(m_layout);

        // optional: push current constants immediately
        pushConstantsNow();
    }

    void ShaderProgram::pushConstantsNow()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();
        if (cmd == VK_NULL_HANDLE)
            return;

        vkCmdPushConstants(cmd, m_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushData), &m_pc);
    }

    // ---- SetUniform overloads ----

    void ShaderProgram::SetUniform(const std::string &name, float v)
    {
        if (name == "u_time")
            m_pc.u_params.x = v;
        else if (name == "u_value")
            m_pc.u_params.y = v;
        else if (name == "u_strength")
            m_pc.u_params.z = v;
        else if (name == "u_color_r")
            m_pc.u_color.r = v;
        else if (name == "u_color_g")
            m_pc.u_color.g = v;
        else if (name == "u_color_b")
            m_pc.u_color.b = v;
        else if (name == "u_color_a")
            m_pc.u_color.a = v;
        else
            return;

        pushConstantsNow();
    }

    void ShaderProgram::SetUniform(const std::string &name, float v0, float v1)
    {
        if (name == "u_params_xy")
        {
            m_pc.u_params.x = v0;
            m_pc.u_params.y = v1;
        }
        else
            return;

        pushConstantsNow();
    }

    void ShaderProgram::SetUniform(const std::string &name, const glm::vec3 &v)
    {
        if (name == "u_color")
        {
            m_pc.u_color = glm::vec4(v, 1.0f);
        }
        else if (name == "uLight.position")
        {
            m_pc.u_lightPos = glm::vec4(v, 1.0f);
        }
        else if (name == "uLight.color")
        {
            m_pc.u_lightColor = glm::vec4(v, 1.0f);
        }
        else if (name == "u_cameraPos")
        {
            m_pc.u_cameraPos = glm::vec4(v, 1.0f);
        }
        else
            return;

        pushConstantsNow();
    }

    void ShaderProgram::SetUniform(const std::string &name, const glm::vec4 &v)
    {
        if (name == "u_color")
        {
            m_pc.u_color = v;
        }
        else if (name == "u_params")
        {
            m_pc.u_params = v;
        }
        else
            return;

        pushConstantsNow();
    }

    void ShaderProgram::SetUniform(const std::string &name, const glm::mat4 &m)
    {
        if (name == "u_model")
        {
            m_pc.u_model = m;
        }
        else
            return;

        pushConstantsNow();
    }

}
