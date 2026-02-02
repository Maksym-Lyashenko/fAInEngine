#include "render/Mesh.h"

#include "graphics/GraphicsAPI.h"
#include "Engine.h"

// #include <cgltf.h>

namespace eng
{
    Mesh::Mesh(const VertexLayout &layout,
               const std::vector<float> &vertices,
               const std::vector<uint32_t> &indices)
    {
        m_vertexLayout = layout;

        auto &api = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = api.CreateVertexBuffer(vertices);
        m_EBO = api.CreateIndexBuffer(indices);

        m_vertexCount = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
        m_indexCount = indices.size();
    }

    Mesh::Mesh(const VertexLayout &layout,
               const std::vector<float> &vertices)
    {
        m_vertexLayout = layout;

        auto &api = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = api.CreateVertexBuffer(vertices);

        m_vertexCount = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
        m_indexCount = 0;
    }

    void Mesh::Bind()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();

        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_VBO, &off);

        if (m_indexCount > 0)
            vkCmdBindIndexBuffer(cmd, m_EBO, 0, VK_INDEX_TYPE_UINT32);
    }

    void Mesh::Draw()
    {
        auto &api = Engine::GetInstance().GetGraphicsAPI();
        VkCommandBuffer cmd = api.GetCmd();

        if (m_indexCount > 0)
            vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        else
            vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
    }

    std::shared_ptr<Mesh> Mesh::CreateCube()
    {
        std::vector<float> vertices =
            {
                // Front face
                0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,

                // Top face
                0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,

                // Right face
                0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
                0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
                0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,

                // Left face
                -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
                -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
                -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,

                // Bottom face
                0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f,
                -0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
                -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
                0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,

                // Back face
                -0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
                0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f,
                0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
                -0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f

            };

        std::vector<unsigned int> indices =
            {
                // front face
                0, 1, 2,
                0, 2, 3,
                // top face
                4, 5, 6,
                4, 6, 7,
                // right face
                8, 9, 10,
                8, 10, 11,
                // left face
                12, 13, 14,
                12, 14, 15,
                // bottom face
                16, 17, 18,
                16, 18, 19,
                // back face
                20, 21, 22,
                20, 22, 23

            };

        eng::VertexLayout layout;
        // Position
        layout.elements.push_back({VertexElement::Position, 3, AttribType::Float32, 0});
        // Color
        layout.elements.push_back({VertexElement::Color, 3, AttribType::Float32, sizeof(float) * 3});
        // UV
        layout.elements.push_back({VertexElement::UV, 2, AttribType::Float32, sizeof(float) * 6});
        // Normal
        layout.elements.push_back({VertexElement::Normal, 3, AttribType::Float32, sizeof(float) * 8});

        layout.stride = sizeof(float) * 11;

        auto result = std::make_shared<eng::Mesh>(layout, vertices, indices);

        return result;
    }

#if 0
    std::shared_ptr<Mesh> Mesh::Load(const std::string &path)
    {
        auto &fs = Engine::GetInstance().GetFileSystem();
        const std::filesystem::path full = fs.GetAssetsFolder() / path;

        SDL_Log("Mesh::Load rel='%s'", path.c_str());
        SDL_Log("Mesh::Load full='%s'", full.string().c_str());
        SDL_Log("exists=%d", std::filesystem::exists(full) ? 1 : 0);

        cgltf_options options{};
        cgltf_data *data = nullptr;

        cgltf_result pres = cgltf_parse_file(&options, full.string().c_str(), &data);
        SDL_Log("cgltf_parse_file res=%d data=%p", (int)pres, (void *)data);

        if (pres != cgltf_result_success || !data)
            return nullptr;

        cgltf_result lres = cgltf_load_buffers(&options, data, full.string().c_str());
        SDL_Log("cgltf_load_buffers res=%d", (int)lres);

        if (lres != cgltf_result_success)
        {
            cgltf_free(data);
            return nullptr;
        }

        SDL_Log("gltf: meshes=%zu nodes=%zu", (size_t)data->meshes_count, (size_t)data->nodes_count);

        std::shared_ptr<Mesh> result = nullptr;

        auto readFloats = [](const cgltf_accessor *acc, cgltf_size i, float *out, int n)
        {
            std::fill(out, out + n, 0.0f);
            return cgltf_accessor_read_float(acc, i, out, n) == 1;
        };

        auto readIndex = [](const cgltf_accessor *acc, cgltf_size i) -> uint32_t
        {
            cgltf_uint out = 0;
            const cgltf_bool ok = cgltf_accessor_read_uint(acc, i, &out, 1);
            return ok ? (uint32_t)out : 0u;
        };

        for (cgltf_size mi = 0; mi < data->meshes_count && !result; ++mi)
        {
            auto &m = data->meshes[mi];
            SDL_Log("mesh[%zu] name='%s' prim=%zu", (size_t)mi, m.name ? m.name : "<noname>", (size_t)m.primitives_count);

            for (cgltf_size pi = 0; pi < m.primitives_count && !result; ++pi)
            {
                auto &prim = m.primitives[pi];
                if (prim.type != cgltf_primitive_type_triangles)
                    continue;

                cgltf_accessor *posAcc = nullptr;
                cgltf_accessor *colAcc = nullptr; // COLOR_0
                cgltf_accessor *uvAcc = nullptr;  // TEXCOORD_0
                cgltf_accessor *nrmAcc = nullptr; // NORMAL

                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
                {
                    auto &attr = prim.attributes[ai];
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

                if (!posAcc)
                    continue;

                const cgltf_size vertexCount = posAcc->count;
                const bool hasUV = (uvAcc != nullptr);
                const bool hasN = (nrmAcc != nullptr);

                SDL_Log("  prim[%zu] vtx=%zu idx=%d uv=%d nrm=%d",
                        (size_t)pi, (size_t)vertexCount, prim.indices ? 1 : 0, hasUV ? 1 : 0, hasN ? 1 : 0);

                // Layout: pos(0) color(1) uv(2) normal(3)
                VertexLayout layout;
                layout.elements.clear();
                layout.stride = 0;

                auto pushEl = [&](uint32_t loc, uint32_t comps)
                {
                    VertexElement e{};
                    e.index = loc;
                    e.size = comps;
                    e.type = AttribType::Float32;
                    e.offset = layout.stride;
                    layout.stride += comps * sizeof(float);
                    layout.elements.push_back(e);
                };

                pushEl(VertexElement::Position, 3);
                pushEl(VertexElement::Color, 3);
                pushEl(VertexElement::UV, 2);
                pushEl(VertexElement::Normal, 3);

                const size_t floatsPerVertex = layout.stride / sizeof(float);
                std::vector<float> vertices((size_t)vertexCount * floatsPerVertex);

                // Fill
                for (cgltf_size vi = 0; vi < vertexCount; ++vi)
                {
                    float *dst = &vertices[(size_t)vi * floatsPerVertex];

                    // pos
                    readFloats(posAcc, vi, dst + 0, 3);

                    // color default white
                    dst[3] = 1.f;
                    dst[4] = 1.f;
                    dst[5] = 1.f;
                    if (colAcc)
                        readFloats(colAcc, vi, dst + 3, 3);

                    // uv default 0
                    dst[6] = 0.f;
                    dst[7] = 0.f;
                    if (uvAcc)
                        readFloats(uvAcc, vi, dst + 6, 2);

                    // normal default (0,0,1)
                    dst[8] = 0.f;
                    dst[9] = 0.f;
                    dst[10] = 1.f;
                    if (nrmAcc)
                        readFloats(nrmAcc, vi, dst + 8, 3);
                }

                // AABB pos
                float mnx = 1e30f, mny = 1e30f, mnz = 1e30f, mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
                for (cgltf_size vi = 0; vi < vertexCount; ++vi)
                {
                    const float *v = &vertices[(size_t)vi * floatsPerVertex];
                    mnx = std::min(mnx, v[0]);
                    mny = std::min(mny, v[1]);
                    mnz = std::min(mnz, v[2]);
                    mxx = std::max(mxx, v[0]);
                    mxy = std::max(mxy, v[1]);
                    mxz = std::max(mxz, v[2]);
                }
                SDL_Log("  AABB min=(%f,%f,%f) max=(%f,%f,%f)", mnx, mny, mnz, mxx, mxy, mxz);

                // Indices
                if (prim.indices)
                {
                    const cgltf_size indexCount = prim.indices->count;
                    std::vector<uint32_t> indices(indexCount);
                    for (cgltf_size i = 0; i < indexCount; ++i)
                        indices[i] = readIndex(prim.indices, i);

                    result = std::make_shared<Mesh>(layout, vertices, indices);
                }
                else
                {
                    result = std::make_shared<Mesh>(layout, vertices);
                }

                SDL_Log("  prim[%zu] created GPU mesh OK", (size_t)pi);
            }
        }

        cgltf_free(data);

        if (!result)
            SDL_Log("Mesh::Load FAILED: no triangle primitive with POSITION");

        return result;
    }
#endif

}
