#include "scene/components/PlayerControllerComponent.h"

#include "input/InputManager.h"
#include "Engine.h"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

namespace eng
{

    void PlayerControllerComponent::Update(float DeltaTime)
    {
        auto &inputManager = Engine::GetInstance().GetInputManager();
        auto rotation = m_owner->GetRotation();

        if (inputManager.IsMouseButtonPressed(SDL_BUTTON_LEFT))
        {
            const auto &oldPos = inputManager.GetMousePositionOld();
            const auto &currentPos = inputManager.GetMousePositionCurrent();

            float deltaX = currentPos.x - oldPos.x;
            float deltaY = currentPos.y - oldPos.y;

            // rotation around Y axis
            rotation.y -= deltaX * m_sensitivity * DeltaTime;

            // rotation around X axis
            rotation.x -= deltaY * m_sensitivity * DeltaTime;

            m_owner->SetRotation(rotation);
        }

        glm::mat4 rotMat(1.f);
        rotMat = glm::rotate(rotMat, rotation.x, glm::vec3(1.f, 0.f, 0.f)); // X-axis
        rotMat = glm::rotate(rotMat, rotation.y, glm::vec3(0.f, 1.f, 0.f)); // Y-axis
        rotMat = glm::rotate(rotMat, rotation.z, glm::vec3(0.f, 0.f, 1.f)); // Z-axis

        glm::vec3 front = glm::normalize(glm::vec3(rotMat * glm::vec4(0.f, 0.f, -1.f, 0.f)));
        glm::vec3 right = glm::normalize(glm::vec3(rotMat * glm::vec4(1.f, 0.f, 0.f, 0.f)));

        auto position = m_owner->GetPosition();

        // Left/Right movement
        if (inputManager.IsKeyPressed(SDL_SCANCODE_A))
        {
            position -= right * m_moveSpeed * DeltaTime;
        }
        else if (inputManager.IsKeyPressed(SDL_SCANCODE_D))
        {
            position += right * m_moveSpeed * DeltaTime;
        }

        // Vertical movement
        if (inputManager.IsKeyPressed(SDL_SCANCODE_W))
        {
            position += front * m_moveSpeed * DeltaTime;
        }
        else if (inputManager.IsKeyPressed(SDL_SCANCODE_S))
        {
            position -= front * m_moveSpeed * DeltaTime;
        }
        m_owner->SetPosition(position);
    }

}