#pragma once

#include "scene/Component.h"

#include <glm/mat4x4.hpp>

namespace eng
{

    class CameraComponent : public Component
    {
        COMPONENT(CameraComponent)

    public:
        void Update(float DeltaTime) override;

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix(float aspect) const;

    private:
        float m_fov = 60.f;
        float m_nearPlane = 0.1f;
        float m_farPlane = 1000.f;
    };

}