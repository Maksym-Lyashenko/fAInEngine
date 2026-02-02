// graphics/VertexLayout.h
#pragma once
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace eng
{

    enum class AttribType : uint8_t
    {
        Float32
    };

    struct VertexElement
    {
        uint32_t index;  // location
        uint32_t size;   // components 1..4
        AttribType type; // сейчас достаточно Float32
        uint32_t offset; // bytes offset

        static constexpr uint32_t Position = 0;
        static constexpr uint32_t Color = 1;
        static constexpr uint32_t UV = 2;
        static constexpr uint32_t Normal = 3;
    };

    struct VertexLayout
    {
        std::vector<VertexElement> elements;
        uint32_t stride = 0;
    };

    inline VkFormat ToVkFormat(AttribType t, uint32_t comps)
    {
        if (t == AttribType::Float32)
        {
            switch (comps)
            {
            case 1:
                return VK_FORMAT_R32_SFLOAT;
            case 2:
                return VK_FORMAT_R32G32_SFLOAT;
            case 3:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case 4:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
        }
        return VK_FORMAT_UNDEFINED;
    }

}
