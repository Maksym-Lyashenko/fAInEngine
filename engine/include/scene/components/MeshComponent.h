#pragma once

#include "scene/Component.h"

#include <memory>

namespace eng
{
    class Material;
    class Mesh;

    class MeshComponent : public Component
    {
    public:
        MeshComponent(const std::shared_ptr<Material> &material, const std::shared_ptr<Mesh> &mesh);
        void Update(float DeltaTime) override;

    private:
        std::shared_ptr<Material> m_material;
        std::shared_ptr<Mesh> m_mesh;
    };

}