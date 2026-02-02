#include "TestObject.h"

#include <SDL3/SDL.h>

#include <iostream>

TestObject::TestObject()
{
    auto &graphicsAPI = eng::Engine::GetInstance().GetGraphicsAPI();

    eng::VertexLayout layout;
    layout.elements.push_back({0, 3, eng::AttribType::Float32, 0});
    layout.elements.push_back({1, 3, eng::AttribType::Float32, sizeof(float) * 3});
    layout.elements.push_back({2, 2, eng::AttribType::Float32, sizeof(float) * 6});
    layout.stride = sizeof(float) * 8;

    auto shaderProgram = graphicsAPI.CreateShaderProgram(
        "assets/shaders/vertex.spv",
        "assets/shaders/fragment.spv",
        layout);

    auto material = std::make_shared<eng::Material>();

    material->SetShaderProgram(shaderProgram);

    std::vector<float> vertices =
        {
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f,

            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f

        };

    std::vector<unsigned int> indices =
        {
            // front face
            0, 1, 2,
            0, 2, 3,
            // top face
            4, 5, 1,
            4, 1, 0,
            // right face
            4, 0, 3,
            4, 3, 7,
            // left face
            1, 5, 6,
            1, 6, 2,
            // bottom face
            3, 2, 6,
            3, 6, 7,
            // back face
            4, 7, 6,
            4, 6, 5

        };

    auto mesh = std::make_shared<eng::Mesh>(layout, vertices, indices);

    AddComponent(new eng::MeshComponent(material, mesh));
}

void TestObject::Update(float DeltaTime)
{
    eng::GameObject::Update(DeltaTime);

#if 0
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
#endif
}
