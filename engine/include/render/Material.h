#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <unordered_map>
#include <string>

namespace eng
{
    class ShaderProgram;
    class Texture;

    class Material
    {
    public:
        void SetShaderProgram(const std::shared_ptr<ShaderProgram> &shaderProgram);
        void SetParam(const std::string &name, float value);
        void SetParam(const std::string &name, float v0, float v1);
        void SetTexture(const std::string &name, const std::shared_ptr<Texture> &texture);
        void Bind();

        static std::shared_ptr<Material> Load(const std::string &path);

        ShaderProgram *GetShaderProgram();

    private:
        std::shared_ptr<ShaderProgram> m_shaderProgram;
        std::unordered_map<std::string, float> m_floatParams;
        std::unordered_map<std::string, std::pair<float, float>> m_float2Params;

        std::shared_ptr<Texture> m_texture;
        std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
        VkDescriptorSet m_textureSet = VK_NULL_HANDLE;
    };
}