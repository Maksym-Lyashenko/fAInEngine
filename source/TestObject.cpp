#include "TestObject.h"

#include <SDL3/SDL.h>

#include <iostream>

TestObject::TestObject()
{
    auto &graphicsAPI = eng::Engine::GetInstance().GetGraphicsAPI();

    eng::VertexLayout layout;
    layout.elements.push_back({0, 3, eng::AttribType::Float32, 0});
    layout.elements.push_back({1, 3, eng::AttribType::Float32, sizeof(float) * 3});

    graphicsAPI.SetClearColor(0.f, 0.f, 0.f, 1.f);

    layout.stride = sizeof(float) * 6;

    auto shaderProgram = graphicsAPI.CreateShaderProgram(
        "assets/shaders/vertex.spv",
        "assets/shaders/fragment.spv",
        layout);

    m_material.SetShaderProgram(shaderProgram);

    std::vector<float> vertices =
        {
            0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f};

    std::vector<unsigned int> indices =
        {
            0, 1, 2,
            0, 2, 3};

    m_mesh = std::make_shared<eng::Mesh>(layout, vertices, indices);
}

void TestObject::Update(float DeltaTime)
{
    eng::GameObject::Update(DeltaTime);

    auto position = GetPosition();
    auto &input = eng::Engine::GetInstance().GetInputManager();

    // Horizontal movement
    if (input.IsKeyPressed(SDL_SCANCODE_A))
    {
        position.x -= 0.01f;
    }
    else if (input.IsKeyPressed(SDL_SCANCODE_D))
    {
        position.x += 0.01f;
    }

    // Vertical movement
    if (input.IsKeyPressed(SDL_SCANCODE_W))
    {
        position.y += 0.01f;
    }
    else if (input.IsKeyPressed(SDL_SCANCODE_S))
    {
        position.y -= 0.01f;
    }
    SetPosition(position);

    eng::RenderCommand command;
    command.material = &m_material;
    command.mesh = m_mesh.get();
    command.modelMatrix = GetWorldTransform();

    auto &renderQueue = eng::Engine::GetInstance().GetRenderQueue();
    renderQueue.Submit(command);
}
