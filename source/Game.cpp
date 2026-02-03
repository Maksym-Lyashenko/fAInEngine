#include "Game.h"

#include "TestObject.h"

#include <iostream>

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb/stb_image.h>

bool Game::Init()
{
    auto &fs = eng::Engine::GetInstance().GetFileSystem();

    m_scene = new eng::Scene();
    eng::Engine::GetInstance().SetScene(m_scene);

    auto camera = m_scene->CreateObject("Camera");
    camera->AddComponent(new eng::CameraComponent());
    camera->SetPosition(glm::vec3(0.f, 0.f, 2.f));
    camera->AddComponent(new eng::PlayerControllerComponent());

    m_scene->SetMainCamera(camera);

    // m_scene->CreateObject<TestObject>("TestObject");

    auto &graphicsAPI = eng::Engine::GetInstance().GetGraphicsAPI();

    graphicsAPI.SetClearColor(1.f, 1.f, 1.f, 1.f);

    auto material = eng::Material::Load("materials/brick.mat");

    auto mesh = eng::Mesh::CreateCube();

    // auto objectA = m_scene->CreateObject("ObjectA");
    // objectA->AddComponent(new eng::MeshComponent(material, mesh));
    // objectA->SetPosition(glm::vec3(1.f, 0.f, -5.f));

    auto objectB = m_scene->CreateObject("ObjectB");
    objectB->AddComponent(new eng::MeshComponent(material, mesh));
    objectB->SetPosition(glm::vec3(0.f, 2.f, 2.f));
    objectB->SetRotation(glm::vec3(0.f, 2.f, 0.f));

    auto objectC = m_scene->CreateObject("ObjectC");
    objectC->AddComponent(new eng::MeshComponent(material, mesh));
    objectC->SetPosition(glm::vec3(-2.f, 0.f, 0.f));
    objectC->SetRotation(glm::vec3(1.f, 0.f, 1.f));
    objectC->SetScale(glm::vec3(1.5f, 1.5f, 1.5f));

    // auto suzanneMesh = eng::Mesh::Load("models/suzanne/Suzanne.gltf");
    // auto suzanneMaterial = eng::Material::Load("materials/suzanne.mat");

    // auto suzanneObj = m_scene->CreateObject("Suzanne");
    // suzanneObj->AddComponent(new eng::MeshComponent(suzanneMaterial, suzanneMesh));
    // suzanneObj->SetPosition(glm::vec3(0.f, 0.f, -5.f));

    auto suzanneObject = eng::GameObject::LoadGLTF("models/suzanne/Suzanne.gltf");
    suzanneObject->SetPosition(glm::vec3(0.f, 0.f, -5.f));

    auto gun = eng::GameObject::LoadGLTF("models/sten_gunmachine_carbine/scene.gltf");
    gun->SetParent(camera);
    gun->SetPosition(glm::vec3(.75f, -.5f, -.75f));
    gun->SetScale(glm::vec3(-1.f, 1.f, 1.f));

    auto makarov = eng::GameObject::LoadGLTF("models/makarov/scene.gltf");
    makarov->SetScale(glm::vec3(.01f, .01f, .01f));

    auto light = m_scene->CreateObject("Light");
    auto lightComponent = new eng::LightComponent();
    lightComponent->SetColor(glm::vec3(1.f));
    light->AddComponent(lightComponent);
    light->SetPosition(glm::vec3(0.f, 5.f, 0.f));

    return true;
}

void Game::Update(float DeltaTime)
{
    m_scene->Update(DeltaTime);
}

void Game::Destroy()
{
}
