#include "render/Mesh.h"

#include "graphics/GraphicsAPI.h"
#include "Engine.h"

namespace eng
{
    Mesh::Mesh(const VertexLayout &layout,
               const std::vector<float> &vertices,
               const std::vector<uint32_t> &indices)
    {
        m_vertexLayout = layout;

        auto &api = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = api.CreateVertexBuffer(vertices);
        m_EBO = api.CreateIndexBuffer(indices);

        m_vertexCount = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
        m_indexCount = indices.size();
    }

    Mesh::Mesh(const VertexLayout &layout,
               const std::vector<float> &vertices)
    {
        m_vertexLayout = layout;

        auto &api = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = api.CreateVertexBuffer(vertices);

        m_vertexCount = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
        m_indexCount = 0;
    }

    void Mesh::Bind()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();

        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_VBO, &off);

        if (m_indexCount > 0)
            vkCmdBindIndexBuffer(cmd, m_EBO, 0, VK_INDEX_TYPE_UINT32);
    }

    void Mesh::Draw()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();

        if (m_indexCount > 0)
            vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        else
            vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
    }

}