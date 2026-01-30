#pragma once

#include <SDL3/SDL.h>

#include <array>

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
        void Clear();

    private:
        std::array<bool, SDL_SCANCODE_COUNT> m_keys = {false};
        friend class Engine;
    };

}