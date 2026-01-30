#include "Game.h"

#include "TestObject.h"

bool Game::Init()
{
    m_scene = new eng::Scene();

    auto camera = m_scene->CreateObject("Camera");
    camera->AddComponent(new eng::CameraComponent());
    camera->SetPosition(glm::vec3(0.f, 0.f, 2.f));

    m_scene->SetMainCamera(camera);

    m_scene->CreateObject<TestObject>("TestObject");

    eng::Engine::GetInstance().SetScene(m_scene);

    return true;
}

void Game::Update(float DeltaTime)
{
    m_scene->Update(DeltaTime);
}

void Game::Destroy()
{
}
