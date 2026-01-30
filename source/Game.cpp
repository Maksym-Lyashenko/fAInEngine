#include "Game.h"

#include "TestObject.h"

bool Game::Init()
{
    m_scene.CreateObject<TestObject>("TestObject");

    return true;
}

void Game::Update(float DeltaTime)
{
    m_scene.Update(DeltaTime);
}

void Game::Destroy()
{
}
