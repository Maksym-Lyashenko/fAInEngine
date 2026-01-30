#include "input/InputManager.h"

namespace eng
{

    void InputManager::SetKeyPressed(SDL_Scancode sc, bool pressed)
    {
        const int key = static_cast<int>(sc);
        if (key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return;
        }

        m_keys[key] = pressed;
    }

    bool InputManager::IsKeyPressed(SDL_Scancode sc)
    {
        const int key = static_cast<int>(sc);
        if (key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return false;
        }

        return m_keys[key];
    }

    void InputManager::SetMouseButtonPressed(int button, bool pressed)
    {
        if (button < 0 || button >= static_cast<int>(m_mouseKeys.size()))
        {
            return;
        }
        m_mouseKeys[button] = pressed;
    }

    bool InputManager::IsMouseButtonPressed(int button)
    {

        if (button < 0 || button >= static_cast<int>(m_mouseKeys.size()))
        {
            return false;
        }
        return m_mouseKeys[button];
    }

    void InputManager::SetMousePositionOld(const glm::vec2 &pos)
    {
        m_mousePositionOld = pos;
    }

    const glm::vec2 &InputManager::GetMousePositionOld() const
    {
        return m_mousePositionOld;
    }

    void InputManager::SetMousePositionCurrent(const glm::vec2 &pos)
    {
        m_mousePositionCurrent = pos;
    }

    const glm::vec2 &eng::InputManager::GetMousePositionCurrent() const
    {
        return m_mousePositionCurrent;
    }

    void InputManager::Clear()
    {
        m_keys.fill(false);
        m_mouseKeys.fill(false);
        m_mousePositionOld = glm::vec2(0.f);
        m_mousePositionCurrent = glm::vec2(0.f);
    }
}