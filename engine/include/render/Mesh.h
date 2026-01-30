#pragma once

#include "graphics/VertexLayout.h"

#include <vulkan/vulkan.h>

namespace eng
{
    class Mesh
    {
    public:
        Mesh(const VertexLayout &layout, const std::vector<float> &vertices, const std::vector<uint32_t> &indices);
        Mesh(const VertexLayout &layout, const std::vector<float> &vertices);
        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        void Bind();
        void Draw();

    private:
        VertexLayout m_vertexLayout;
        VkBuffer m_VBO = VK_NULL_HANDLE;
        VkBuffer m_EBO = VK_NULL_HANDLE;

        size_t m_vertexCount = 0;
        size_t m_indexCount = 0;
    };
}