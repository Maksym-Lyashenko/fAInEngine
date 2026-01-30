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

    void InputManager::Clear()
    {
        m_keys.fill(false);
    }
}