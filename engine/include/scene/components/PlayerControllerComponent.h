#pragma once

#include "scene/Component.h"

namespace eng
{

    class PlayerControllerComponent : public Component
    {
        COMPONENT(PlayerControllerComponent)
    public:
        void Update(float DeltaTime) override;

    private:
        float m_sensitivity = 0.002f;
        float m_moveSpeed = 3.f;
    };

}