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
        auto &input = Engine::GetInstance().GetInputManager();

        auto rotation = m_owner->GetRotation(); // radians

        // Mouse look (LMB)
        if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT))
        {
            const auto &oldPos = input.GetMousePositionOld();
            const auto &curPos = input.GetMousePositionCurrent();

            float dx = curPos.x - oldPos.x;
            float dy = curPos.y - oldPos.y;

            // rotation.y -= dx * m_sensitivity; // yaw
            float yAngle = -dx * m_sensitivity;
            glm::quat yRot = glm::angleAxis(yAngle, glm::vec3(0.f, 1.f, 0.f));

            // rotation.x -= dy * m_sensitivity; // pitch
            float XAngle = -dy * m_sensitivity;
            glm::vec3 right = rotation * glm::vec3(1.f, 0.f, 0.f);
            glm::quat xRot = glm::angleAxis(XAngle, right);

            glm::quat deltaRot = yRot * xRot;
            rotation = glm::normalize(deltaRot * rotation);
            // rotation.x = glm::clamp(rotation.x, glm::radians(-89.0f), glm::radians(89.0f));
            m_owner->SetRotation(rotation);
        }

        glm::vec3 front = rotation * glm::vec3(0.f, 0.f, -1.f);
        glm::vec3 right = rotation * glm::vec3(1.f, 0.f, 0.f);

        // Move
        glm::vec3 move(0.0f);
        if (input.IsKeyPressed(SDL_SCANCODE_A))
            move -= right;
        if (input.IsKeyPressed(SDL_SCANCODE_D))
            move += right;
        if (input.IsKeyPressed(SDL_SCANCODE_W))
            move += front;
        if (input.IsKeyPressed(SDL_SCANCODE_S))
            move -= front;

        auto position = m_owner->GetPosition();

        if (glm::length(move) > 0.0f)
            position += glm::normalize(move) * m_moveSpeed * DeltaTime;

        m_owner->SetPosition(position);
    }
}