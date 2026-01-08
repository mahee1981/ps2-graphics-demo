#include "renderer/model.hpp"
#include "logging/log.hpp"
#include "utils.hpp"
#include <iostream>

namespace Renderer
{

Model::Model(const ps2math::Vec4 &position) : _transformComponent(position)
{
}

void Model::Update()
{
    ps2math::Mat4 work;

    float scaleFactor = _transformComponent.GetScaleFactor();
    work = ps2math::Mat4::scale(work, ps2math::Vec4{scaleFactor, scaleFactor, scaleFactor, 1.0f});

    work = ps2math::Mat4::rotateX(work, Utils::ToRadians(_transformComponent.GetAngleX()));

    work = ps2math::Mat4::rotateY(work, Utils::ToRadians(_transformComponent.GetAngleY()));

    work = ps2math::Mat4::rotateZ(work, Utils::ToRadians(_transformComponent.GetAngleZ()));

    work = ps2math::Mat4::translate(work, _transformComponent.GetTranslate());

    _worldMatrix = work;
}

// TODO: Move to a system maybe?

void Model::LoadModel(const char *fileName, const char *material_search_path)
{
    tinyobj::ObjReaderConfig reader_config;
    if (material_search_path != nullptr)
        reader_config.mtl_search_path = material_search_path; // Path to material files
    reader_config.vertex_color = true;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(fileName, reader_config))
    {
        if (!reader.Error().empty())
        {
            LOG_ERROR("TinyObjReader: ") << reader.Error();
        }
    }

    if (!reader.Warning().empty())
    {
        LOG_WARNING("TinyObjReader: ") << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    // auto &materials = reader.GetMaterials();
    //
    LOG_INFO("Number of shapes: ") << shapes.size();
    // Loop over shapes and store the indices
    //
    for (size_t s = 0; s < shapes.size(); s++)
    {
        Mesh newMesh;
        newMesh.Vertices.reserve(shapes[s].mesh.indices.size());
        newMesh.Normals.reserve(shapes[s].mesh.indices.size());
        newMesh.Texels.reserve(shapes[s].mesh.indices.size());
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                newMesh.Vertices.emplace_back(ps2math::Vec4{vx, vy, vz, 1.0f});

                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0)
                {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    newMesh.Normals.emplace_back(ps2math::Vec4{nx, ny, nz, 0.0f});
                }
                else
                {
                    LOG_ERROR("No normal found for vertex, adding default");
                    newMesh.Normals.emplace_back(ps2math::Vec4{1.0f, 1.0f, 1.0f, 0.0f});
                }

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0)
                {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    texel_t texel;
                    texel.u = tx;
                    texel.v = ty;
                    newMesh.Texels.emplace_back(std::move(texel));
                }
                else
                {
                    newMesh.Texels.emplace_back(texel_t{});
                }
            }
            index_offset += fv;
        }
        meshList.push_back(newMesh);
    }
}

} // namespace Renderer
