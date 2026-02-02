#include "Engine.h"

#include <SDL3/SDL.h>

#include "Application.h"

namespace eng
{

    void keyboardHandler(const SDL_Event &e)
    {
        auto &inputManager = Engine::GetInstance().GetInputManager();

        if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
        {
            // e.key.scancode — SDL_Scancode
            // e.key.down     — bool (true for down)
            // e.key.repeat   — auto repeat (if needs to ignore)
            if (!e.key.repeat)
            {
                inputManager.SetKeyPressed(e.key.scancode, e.key.down);
            }
        }
    }

    void mouseHandler(const SDL_Event &e)
    {
        auto &inputManager = Engine::GetInstance().GetInputManager();

        if (e.type == SDL_EVENT_MOUSE_MOTION)
        {
            inputManager.SetMousePositionCurrent({(float)e.motion.x, (float)e.motion.y});
            return;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            inputManager.SetMouseButtonPressed(e.button.button, true);
            return;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            inputManager.SetMouseButtonPressed(e.button.button, false);
            return;
        }

        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            inputManager.Clear();
            return;
        }
    }

    Engine &Engine::GetInstance()
    {
        static Engine instance;
        return instance;
    }

    bool Engine::Init(int width, int height)
    {
        if (!m_application)
        {
            return false;
        }

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        m_window = SDL_CreateWindow(
            "fAInEngine",
            width, height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

        if (!m_window)
        {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            SDL_Quit();
            return false;
        }
        m_vulkanContext.init(m_window);
        return m_application->Init();
    }

    void Engine::Run()
    {
        if (!m_application)
        {
            return;
        }

        m_lastTimePoint = std::chrono::high_resolution_clock::now();

        bool running = true;
        bool resized = false;
        while (running && !m_application->NeedsToBeClosed())
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                {
                    running = false;
                }

                keyboardHandler(e);
                mouseHandler(e);

                if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST || e.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    m_inputManager.Clear(); // чтобы не залипали клавиши
                }

                if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                    e.type == SDL_EVENT_WINDOW_RESIZED)
                {
                    resized = true;
                }
            }

            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(now - m_lastTimePoint).count();
            m_lastTimePoint = now;

            m_application->Update(deltaTime);

            m_vulkanContext.drawFrame(m_window, resized);
            m_inputManager.SetMousePositionOld(m_inputManager.GetMousePositionCurrent());
            resized = false;
        }

        m_vulkanContext.waitIdle();
    }

    void Engine::Destroy()
    {
        if (m_application)
        {
            m_application->Destroy();
            m_application.reset();
        }

        m_vulkanContext.waitIdle();
        m_graphicsAPI.DestroyBuffers();

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
    }

    void Engine::SetApplication(Application *app)
    {
        m_application.reset(app);
    }

    Application *Engine::GetApplication()
    {
        return m_application.get();
    }

    InputManager &Engine::GetInputManager()
    {
        return m_inputManager;
    }

    VulkanContext &Engine::GetVulkanContext()
    {
        return m_vulkanContext;
    }

    GraphicsAPI &Engine::GetGraphicsAPI()
    {
        return m_graphicsAPI;
    }

    RenderQueue &Engine::GetRenderQueue()
    {
        return m_renderQueue;
    }

    FileSystem &Engine::GetFileSystem()
    {
        return m_fileSystem;
    }

    TextureManager &Engine::GetTextureManager()
    {
        return m_textureManager;
    }

    void Engine::SetScene(Scene *scene)
    {
        m_currentScene.reset(scene);
    }

    Scene *Engine::GetScene()
    {
        return m_currentScene.get();
    }
}
