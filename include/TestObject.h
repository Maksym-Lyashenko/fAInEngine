#pragma once

#include <eng.h>

#include <memory>

class TestObject : public eng::GameObject
{
public:
    TestObject();

    void Update(float DeltaTime) override;

private:
    eng::Material m_material;
    std::shared_ptr<eng::Mesh> m_mesh;
};