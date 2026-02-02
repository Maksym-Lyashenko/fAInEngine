#include "scene/GameObject.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cgltf.h>

#include "Engine.h"
#include "graphics/Texture.h"
#include "graphics/VertexLayout.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "scene/components/MeshComponent.h"

namespace eng
{

    void GameObject::Update(float DeltaTime)
    {
        for (auto &component : m_components)
        {
            component->Update(DeltaTime);
        }

        for (auto it = m_children.begin(); it != m_children.end();)
        {
            if ((*it)->IsAlive())
            {
                (*it)->Update(DeltaTime);
                ++it;
            }
            else
            {
                it = m_children.erase(it);
            }
        }
    }

    const std::string &GameObject::GetName() const
    {
        return m_name;
    }

    void GameObject::SetName(const std::string &name)
    {
        m_name = name;
    }

    GameObject *GameObject::GetParent()
    {
        return m_parent;
    }

    bool GameObject::SetParent(GameObject *parent)
    {
        if (!m_scene)
        {
            return false;
        }

        return m_scene->SetParent(this, parent);
    }

    Scene *GameObject::GetScene()
    {
        return m_scene;
    }

    bool GameObject::IsAlive() const
    {
        return m_isAlive;
    }

    void GameObject::MarkForDestroy()
    {
        m_isAlive = false;
    }

    void GameObject::AddComponent(Component *component)
    {
        m_components.emplace_back(component);
        component->m_owner = this;
    }

    const glm::vec3 &GameObject::GetPosition() const
    {
        return m_position;
    }

    glm::vec3 GameObject::GetWordPosition() const
    {
        glm::vec4 hom = GetWorldTransform() * glm::vec4(0.f, 0.f, 0.f, 1.f);
        return glm::vec3(hom) / hom.w;
    }

    void GameObject::SetPosition(const glm::vec3 &pos)
    {
        m_position = pos;
    }

    const glm::quat &GameObject::GetRotation() const
    {
        return m_rotation;
    }

    void GameObject::SetRotation(const glm::quat &rot)
    {
        m_rotation = rot;
    }

    const glm::vec3 &GameObject::GetScale() const
    {
        return m_scale;
    }

    void GameObject::SetScale(const glm::vec3 &scale)
    {
        m_scale = scale;
    }

    glm::mat4 GameObject::GetLocalTransform() const
    {
        glm::mat4 mat = glm::mat4(1.f);

        // Translation
        mat = glm::translate(mat, m_position);

        // Rotation
        mat = mat * glm::mat4_cast(m_rotation);

        // Scale
        mat = glm::scale(mat, m_scale);

        return mat;
    }

    glm::mat4 GameObject::GetWorldTransform() const
    {
        if (m_parent)
        {
            return m_parent->GetWorldTransform() * GetLocalTransform();
        }
        else
        {
            return GetLocalTransform();
        }
    }

    // ---- helpers ----

    static VertexLayout MakeDefaultLayout_PosColUvNrm()
    {
        VertexLayout layout;
        layout.elements.clear();
        layout.elements.push_back({VertexElement::Position, 3, AttribType::Float32, 0});
        layout.elements.push_back({VertexElement::Color, 3, AttribType::Float32, sizeof(float) * 3});
        layout.elements.push_back({VertexElement::UV, 2, AttribType::Float32, sizeof(float) * 6});
        layout.elements.push_back({VertexElement::Normal, 3, AttribType::Float32, sizeof(float) * 8});
        layout.stride = sizeof(float) * 11;
        return layout;
    }

    static void ApplyNodeTRS(const cgltf_node *node, GameObject *obj)
    {
        if (!node || !obj)
            return;

        if (node->has_matrix)
        {
            glm::mat4 m = glm::make_mat4(node->matrix);

            glm::vec3 translation, scale, skew;
            glm::vec4 perspective;
            glm::quat orientation;

            glm::decompose(m, scale, orientation, translation, skew, perspective);

            obj->SetPosition(translation);
            obj->SetRotation(glm::normalize(orientation));
            obj->SetScale(scale);
        }
        else
        {
            if (node->has_translation)
                obj->SetPosition(glm::vec3(node->translation[0], node->translation[1], node->translation[2]));

            if (node->has_rotation)
            {
                glm::quat q(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
                obj->SetRotation(glm::normalize(q));
            }

            if (node->has_scale)
                obj->SetScale(glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
        }
    }

    static bool ReadFloats(const cgltf_accessor *acc, cgltf_size i, float *out, int n)
    {
        std::fill(out, out + n, 0.0f);
        return cgltf_accessor_read_float(acc, i, out, n) == 1;
    }

    static uint32_t ReadIndex(const cgltf_accessor *acc, cgltf_size i)
    {
        cgltf_uint out = 0;
        const cgltf_bool ok = cgltf_accessor_read_uint(acc, i, &out, 1);
        return ok ? (uint32_t)out : 0u;
    }

    static std::shared_ptr<Texture> LoadTextureCached(
        const std::filesystem::path &absPath,
        std::unordered_map<std::string, std::shared_ptr<Texture>> &cache)
    {
        const std::string key = absPath.lexically_normal().string();
        if (auto it = cache.find(key); it != cache.end())
            return it->second;

        auto tex = Engine::GetInstance().GetTextureManager().GetOrLoadTexture(key);

        cache[key] = tex;
        return tex;
    }

    static std::shared_ptr<Material> BuildMaterialForPrimitive(
        const cgltf_primitive &prim,
        const std::filesystem::path &gltfFolderAbs,
        std::unordered_map<std::string, std::shared_ptr<Texture>> &texCache)
    {
        auto mat = std::make_shared<Material>();

        auto &api = Engine::GetInstance().GetGraphicsAPI();
        mat->SetShaderProgram(api.GetDefaultShaderProgram());

        if (prim.material)
        {
            const cgltf_material *m = prim.material;
            const cgltf_texture *baseTex = nullptr;

            if (m->has_pbr_metallic_roughness)
            {
                const auto &pbr = m->pbr_metallic_roughness;
                baseTex = pbr.base_color_texture.texture;
            }
            else if (m->has_pbr_specular_glossiness)
            {
                const auto &pbr = m->pbr_specular_glossiness;
                baseTex = pbr.diffuse_texture.texture;
            }

            if (baseTex && baseTex->image && baseTex->image->uri)
            {
                std::filesystem::path imgRel(baseTex->image->uri);
                std::filesystem::path imgAbs = imgRel.is_absolute() ? imgRel : (gltfFolderAbs / imgRel);

                auto tex = LoadTextureCached(imgAbs, texCache);

                mat->SetTexture("baseColorTexture", tex);
            }
        }

        return mat;
    }

    static std::shared_ptr<Mesh> BuildMeshForPrimitive(const cgltf_primitive &prim)
    {
        if (prim.type != cgltf_primitive_type_triangles)
            return nullptr;

        const cgltf_accessor *posAcc = nullptr;
        const cgltf_accessor *colAcc = nullptr; // COLOR_0
        const cgltf_accessor *uvAcc = nullptr;  // TEXCOORD_0
        const cgltf_accessor *nrmAcc = nullptr; // NORMAL

        for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
        {
            const auto &attr = prim.attributes[ai];
            if (!attr.data)
                continue;

            if (attr.type == cgltf_attribute_type_position)
                posAcc = attr.data;
            else if (attr.type == cgltf_attribute_type_color && attr.index == 0)
                colAcc = attr.data;
            else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
                uvAcc = attr.data;
            else if (attr.type == cgltf_attribute_type_normal)
                nrmAcc = attr.data;
        }

        if (!posAcc || posAcc->count == 0)
            return nullptr;

        const cgltf_size vcount = posAcc->count;

        VertexLayout layout = MakeDefaultLayout_PosColUvNrm();
        const size_t floatsPerVertex = layout.stride / sizeof(float);

        std::vector<float> vertices((size_t)vcount * floatsPerVertex);

        for (cgltf_size vi = 0; vi < vcount; ++vi)
        {
            float *dst = &vertices[(size_t)vi * floatsPerVertex];

            // pos
            ReadFloats(posAcc, vi, dst + 0, 3);

            // color default white
            dst[3] = 1.f;
            dst[4] = 1.f;
            dst[5] = 1.f;
            if (colAcc)
            {
                float tmp[4]{1, 1, 1, 1};
                ReadFloats(colAcc, vi, tmp, 4);
                dst[3] = tmp[0];
                dst[4] = tmp[1];
                dst[5] = tmp[2];
            }

            // uv default 0
            dst[6] = 0.f;
            dst[7] = 0.f;
            if (uvAcc)
                ReadFloats(uvAcc, vi, dst + 6, 2);

            // normal default (0,0,1)
            dst[8] = 0.f;
            dst[9] = 0.f;
            dst[10] = 1.f;
            if (nrmAcc)
                ReadFloats(nrmAcc, vi, dst + 8, 3);
        }

        std::vector<uint32_t> indices;
        if (prim.indices)
        {
            indices.resize((size_t)prim.indices->count);
            for (cgltf_size i = 0; i < prim.indices->count; ++i)
                indices[(size_t)i] = ReadIndex(prim.indices, i);
        }
        else
        {
            indices.resize((size_t)vcount);
            for (size_t i = 0; i < (size_t)vcount; ++i)
                indices[i] = (uint32_t)i;
        }

        return std::make_shared<Mesh>(layout, vertices, indices);
    }

    static void ParseGLTFNode(
        const cgltf_node *node,
        GameObject *parent,
        const std::filesystem::path &gltfFolderAbs,
        std::unordered_map<std::string, std::shared_ptr<Texture>> &texCache)
    {
        if (!node || !parent)
            return;

        const char *nm = node->name ? node->name : "GLTF_Node";

        auto obj = parent->GetScene()->CreateObject(nm, parent);
        ApplyNodeTRS(node, obj);

        if (node->mesh)
        {
            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi)
            {
                const cgltf_primitive &prim = node->mesh->primitives[pi];
                if (prim.type != cgltf_primitive_type_triangles)
                    continue;

                auto mesh = BuildMeshForPrimitive(prim);
                if (!mesh)
                    continue;

                auto mat = BuildMaterialForPrimitive(prim, gltfFolderAbs, texCache);

                GameObject *owner = obj;
                if (node->mesh->primitives_count > 1)
                {
                    owner = parent->GetScene()->CreateObject((std::string(nm) + "_prim" + std::to_string((size_t)pi)).c_str(), obj);
                }

                owner->AddComponent(new MeshComponent(mat, mesh));
            }
        }

        for (cgltf_size ci = 0; ci < node->children_count; ++ci)
            ParseGLTFNode(node->children[ci], obj, gltfFolderAbs, texCache);
    }

    // ---- public ----

    GameObject *GameObject::LoadGLTF(const std::string &assetPath)
    {
        auto &fs = Engine::GetInstance().GetFileSystem();
        const std::filesystem::path full = fs.GetAssetsFolder() / assetPath;

        if (!std::filesystem::exists(full))
        {
            SDL_Log("GameObject::LoadGLTF FAILED: not found: %s", full.string().c_str());
            return nullptr;
        }

        cgltf_options options{};
        cgltf_data *data = nullptr;

        cgltf_result pres = cgltf_parse_file(&options, full.string().c_str(), &data);
        if (pres != cgltf_result_success || !data)
        {
            SDL_Log("GameObject::LoadGLTF FAILED: cgltf_parse_file res=%d", (int)pres);
            return nullptr;
        }

        cgltf_result lres = cgltf_load_buffers(&options, data, full.string().c_str());
        if (lres != cgltf_result_success)
        {
            SDL_Log("GameObject::LoadGLTF FAILED: cgltf_load_buffers res=%d", (int)lres);
            cgltf_free(data);
            return nullptr;
        }

        const cgltf_scene *gltfScene = data->scene ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
        if (!gltfScene)
        {
            SDL_Log("GameObject::LoadGLTF FAILED: no scene");
            cgltf_free(data);
            return nullptr;
        }

        Scene *scene = Engine::GetInstance().GetScene();
        if (!scene)
        {
            SDL_Log("GameObject::LoadGLTF FAILED: Engine has no active Scene");
            cgltf_free(data);
            return nullptr;
        }

        GameObject *root = scene->CreateObject(("GLTF_" + full.stem().string()).c_str(), nullptr);

        std::unordered_map<std::string, std::shared_ptr<Texture>> texCache;
        const std::filesystem::path folderAbs = full.parent_path();

        for (cgltf_size i = 0; i < gltfScene->nodes_count; ++i)
            ParseGLTFNode(gltfScene->nodes[i], root, folderAbs, texCache);

        cgltf_free(data);
        return root;
    }

}
