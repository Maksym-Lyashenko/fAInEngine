#include "render/Material.h"

#include "graphics/ShaderProgram.h"
#include "Engine.h"
#include "graphics/Texture.h"

#include <nlohmann/json.hpp>

namespace eng
{
    void Material::SetShaderProgram(const std::shared_ptr<ShaderProgram> &shaderProgram)
    {
        m_shaderProgram = shaderProgram;
    }

    void Material::SetParam(const std::string &name, float value)
    {
        m_floatParams[name] = value;
    }

    void Material::SetParam(const std::string &name, float v0, float v1)
    {
        m_float2Params[name] = {v0, v1};
    }

    void Material::SetTexture(const std::string &name, const std::shared_ptr<Texture> &texture)
    {
        m_texture = texture;
        m_textureSet = VK_NULL_HANDLE;

        if (!m_texture)
            return;

        auto &vk = Engine::GetInstance().GetVulkanContext();
        m_textureSet = vk.CreateTextureSet(m_texture->View(), m_texture->Sampler());
    }

    void Material::Bind()
    {
        if (!m_shaderProgram)
            return;

        auto &api = Engine::GetInstance().GetGraphicsAPI();
        api.SetCurrentTextureSet(m_textureSet);

        m_shaderProgram->Bind();

        for (auto &param : m_floatParams)
        {
            m_shaderProgram->SetUniform(param.first, param.second);
        }

        for (auto &param : m_float2Params)
        {
            m_shaderProgram->SetUniform(param.first, param.second.first, param.second.second);
        }
    }

    std::shared_ptr<Material> Material::Load(const std::string &path)
    {
        auto contents = Engine::GetInstance().GetFileSystem().LoadAssetFileText(path);

        if (contents.empty())
        {
            return nullptr;
        }

        nlohmann::json json = nlohmann::json::parse(contents);
        std::shared_ptr<Material> result;

        if (json.contains("shader"))
        {
            auto shaderObj = json["shader"];
            std::string vertexPath = shaderObj.value("vertex", "");
            std::string fragmentPath = shaderObj.value("fragment", "");

            auto &graphicsAPI = Engine::GetInstance().GetGraphicsAPI();
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
            auto shaderProgram = graphicsAPI.CreateShaderProgram(vertexPath, fragmentPath, layout);
            if (!shaderProgram)
            {
                return nullptr;
            }

            result = std::make_shared<Material>();
            result->SetShaderProgram(shaderProgram);
        }

        if (json.contains("params"))
        {
            auto paramsObj = json["params"];

            // Floats
            if (paramsObj.contains("float"))
            {
                for (auto &p : paramsObj["float"])
                {
                    std::string name = p.value("name", "");
                    float value = p.value("value", 0.f);
                    result->SetParam(name, value);
                }
            }

            // Float2
            if (paramsObj.contains("float2"))
            {
                for (auto &p : paramsObj["float2"])
                {
                    std::string name = p.value("name", "");
                    float v0 = p.value("value0", 0.f);
                    float v1 = p.value("value1", 0.f);
                    result->SetParam(name, v0, v1);
                }
            }

            // Textures
            if (paramsObj.contains("textures"))
            {
                for (auto &p : paramsObj["textures"])
                {
                    std::string name = p.value("name", "");
                    std::string texPath = p.value("path", "");
                    auto &vk = Engine::GetInstance().GetVulkanContext();
                    auto texture = Engine::GetInstance().GetTextureManager().GetOrLoadTexture(texPath);

                    result->SetTexture(name, texture);
                }
            }
        }

        return result;
    }

    ShaderProgram *Material::GetShaderProgram()
    {
        return m_shaderProgram.get();
    }
}
