#include "graphics/ShaderProgram.h"
#include "Engine.h"
#include "graphics/GraphicsAPI.h"
#include <fstream>
#include <vector>
#include <stdexcept>

namespace eng
{

    ShaderProgram::~ShaderProgram()
    {
        Destroy();
    }

    static void vkCheck(VkResult r, const char *msg)
    {
        if (r != VK_SUCCESS)
            throw std::runtime_error(msg);
    }

    static std::vector<uint32_t> readSpv(const std::string &path)
    {
        std::ifstream f(path, std::ios::ate | std::ios::binary);
        if (!f)
            throw std::runtime_error("Failed to open spv: " + path);
        size_t sz = (size_t)f.tellg();
        if (sz % 4 != 0)
            throw std::runtime_error("SPV size not multiple of 4: " + path);
        std::vector<uint32_t> data(sz / 4);
        f.seekg(0);
        f.read(reinterpret_cast<char *>(data.data()), sz);
        return data;
    }

    VkShaderModule ShaderProgram::loadModule(const std::string &spvPath)
    {
        auto code = readSpv(spvPath);
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

        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0;
        range.size = sizeof(PushData);

        VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &range;

        vkCheck(vkCreatePipelineLayout(m_device, &li, nullptr, &m_layout),
                "vkCreatePipelineLayout failed");
    }

    void ShaderProgram::Create(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                               const VertexLayout &layout,
                               const std::string &vertSpv, const std::string &fragSpv)
    {
        m_device = device;
        m_renderPass = renderPass;
        m_extent = extent;
        m_vlayout = layout;
        m_vertPath = vertSpv;
        m_fragPath = fragSpv;

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
        rs.cullMode = VK_CULL_MODE_BACK_BIT;    // <-- safest for now (no silent cull)
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE; // doesn't matter if cull none
        rs.lineWidth = 1.f;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
        gp.subpass = 0;

        vkCheck(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline),
                "vkCreateGraphicsPipelines failed");

        vkDestroyShaderModule(m_device, vert, nullptr);
        vkDestroyShaderModule(m_device, frag, nullptr);
    }

    void ShaderProgram::Destroy()
    {
        if (!m_device)
            return;
        if (m_pipeline)
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
        if (m_layout)
            vkDestroyPipelineLayout(m_device, m_layout, nullptr);
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
        if (name == "u_mvp")
        {
            m_pc.u_mvp = m;
        }
        else
            return;

        pushConstantsNow();
    }

}
