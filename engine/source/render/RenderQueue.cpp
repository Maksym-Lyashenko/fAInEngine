#include "render/RenderQueue.h"

#include "render/Mesh.h"
#include "render/Material.h"
#include "graphics/GraphicsAPI.h"
#include "graphics/ShaderProgram.h"

namespace eng
{
    void RenderQueue::Submit(const RenderCommand &command)
    {
        m_commands.push_back(command);
    }

    void RenderQueue::Draw(GraphicsAPI &graphicsAPI, const CameraData &cameraData, const std::vector<LightData> &lights)
    {
        for (auto &command : m_commands)
        {
            graphicsAPI.BindMaterial(command.material);
            auto shaderProgram = command.material->GetShaderProgram();
            shaderProgram->SetUniform("u_model", command.modelMatrix);
            shaderProgram->SetUniform("u_camera_Pos", cameraData.position);
            if (!lights.empty())
            {
                auto &light = lights[0];
                shaderProgram->SetUniform("uLight.color", light.color);
                shaderProgram->SetUniform("uLight.position", light.position);
            }

            graphicsAPI.BindMesh(command.mesh);
            graphicsAPI.DrawMesh(command.mesh);
        }

        m_commands.clear();
    }
}