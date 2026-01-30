#pragma once

#include <SDL3/SDL.h>

#include <array>

#include <glm/vec2.hpp>

namespace eng
{

    class InputManager
    {
    private:
        InputManager() = default;
        InputManager(const InputManager &) = delete;
        InputManager(InputManager &&) = delete;
        InputManager &operator=(const InputManager &) = delete;
        InputManager &operator=(InputManager &&) = delete;

    public:
        void SetKeyPressed(SDL_Scancode sc, bool pressed);
        bool IsKeyPressed(SDL_Scancode sc);

        void SetMouseButtonPressed(int button, bool pressed);
        bool IsMouseButtonPressed(int button);

        void SetMousePositionOld(const glm::vec2 &pos);
        const glm::vec2 &GetMousePositionOld() const;

        void SetMousePositionCurrent(const glm::vec2 &pos);
        const glm::vec2 &GetMousePositionCurrent() const;

        void Clear();

    private:
        std::array<bool, SDL_SCANCODE_COUNT> m_keys = {false};
        std::array<bool, 16> m_mouseKeys = {false};
        glm::vec2 m_mousePositionOld = glm::vec2(0.f);
        glm::vec2 m_mousePositionCurrent = glm::vec2(0.f);

        friend class Engine;
    };

}