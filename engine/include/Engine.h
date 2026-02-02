#pragma once

#include "input/InputManager.h"
#include "graphics/GraphicsAPI.h"
#include "graphics/Texture.h"
#include "vk/VulkanContext.h"
#include "render/RenderQueue.h"
#include "scene/Scene.h"
#include "io/FileSystem.h"

#include <memory>
#include <chrono>

struct SDL_Window;

namespace eng
{
    class Application;

    class Engine
    {
    public:
        static Engine &GetInstance();

    private:
        Engine() = default;
        Engine(const Engine &) = delete;
        Engine(Engine &&) = delete;
        Engine &operator=(const Engine &) = delete;
        Engine &operator=(Engine &&) = delete;

    public:
        bool Init(int width, int height);
        void Run();
        void Destroy();

        void SetApplication(Application *app);
        Application *GetApplication();
        InputManager &GetInputManager();
        VulkanContext &GetVulkanContext();
        GraphicsAPI &GetGraphicsAPI();
        RenderQueue &GetRenderQueue();
        FileSystem &GetFileSystem();
        TextureManager &GetTextureManager();

        void SetScene(Scene *scene);
        Scene *GetScene();

    private:
        std::unique_ptr<Application> m_application;
        std::chrono::steady_clock::time_point m_lastTimePoint;
        SDL_Window *m_window = nullptr;
        InputManager m_inputManager;
        VulkanContext m_vulkanContext;
        GraphicsAPI m_graphicsAPI;
        RenderQueue m_renderQueue;
        FileSystem m_fileSystem;
        TextureManager m_textureManager;
        std::unique_ptr<Scene> m_currentScene;
    };
}