#include "renderer/model.hpp"
#include "logging/log.hpp"
#include "renderer/renderer3d.hpp"
#include "utils.hpp"
#include <algorithm>
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
std::vector<ps2math::Vec4> Model::TransformVertices(const ps2math::Mat4 &mvp,
                                                    float width,
                                                    float height,
                                                    float xOff,
                                                    float yOff)
{
    std::vector<ps2math::Vec4> transformedVertices;
    transformedVertices.reserve(_vertexPositionCoord.size());
    std::transform(_vertexPositionCoord.begin(),
                   _vertexPositionCoord.end(),
                   std::back_inserter(transformedVertices),
                   [=, &mvp](ps2math::Vec4 vertex) {
                       vertex = vertex * mvp;
                       // clip vertices and perform perspective division
                       Renderer3D::ClipVertex(vertex);

                       // viewport transformation
                       float winX = float(width) * vertex.x / 2.0f + (xOff);
                       float winY = float(height) * vertex.y / 2.0f + (yOff);
                       float deviceZ = (vertex.z + 1.0f) / 2.0f * (1 << 31);
                       return ps2math::Vec4(winX, winY, deviceZ, 1.0f);
                   });

    return transformedVertices;
}

void Model::LoadModel(const char *fileName, const char *material_search_path)
{
    tinyobj::ObjReaderConfig reader_config;
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
    // Store the mesh data
    _texCoordinates.reserve(attrib.texcoords.size() / 2);
    _vertexPositionCoord.reserve(attrib.vertices.size() / 3);
    // _vertexColorCoord.reserve(attrib.vertices.size() / 3);
    for (size_t vi = 0; vi < attrib.vertices.size() / 3; ++vi)
    {
        tinyobj::real_t vx = attrib.vertices[3 * vi + 0];
        tinyobj::real_t vy = attrib.vertices[3 * vi + 1];
        tinyobj::real_t vz = attrib.vertices[3 * vi + 2];
        _vertexPositionCoord.emplace_back(vx, vy, vz, 1.0f);

        // tinyobj::real_t cx = attrib.vertices[3 * vi + 0];
        // tinyobj::real_t cy = attrib.vertices[3 * vi + 1];
        // tinyobj::real_t cz = attrib.vertices[3 * vi + 2];
        // _vertexColorCoord.emplace_back(cx, cy, cz, 1.0f);
    }

    for (size_t vi = 0; vi < attrib.texcoords.size() / 2; ++vi)
    {
        texel_t texel;
        texel.u = attrib.texcoords[2 * vi + 0];
        texel.v = attrib.texcoords[2 * vi + 1];
        _texCoordinates.push_back(texel);
    }
    LOG_INFO("Number of shapes: ") << shapes.size();
    LOG_INFO("Total number of vertices in model: ") << this->GetVertexPositions().size();
    LOG_INFO("Number of texels: ") << this->GetTexturePositions().size();
    // Loop over shapes and store the indices
    for (size_t s = 0; s < shapes.size(); s++)
    {
        Mesh newMesh;
        newMesh.TexIndices.reserve(shapes[s].mesh.indices.size());
        newMesh.VertexIndices.reserve(shapes[s].mesh.indices.size());

        for (const auto &idx : shapes[s].mesh.indices)
        {
            newMesh.VertexIndices.push_back(idx.vertex_index);
            if (idx.texcoord_index >= 0)
            {
                newMesh.TexIndices.push_back(idx.texcoord_index);
            }
            else
            {
                newMesh.TexIndices.push_back(-1); // mark missing texcoord
            }
        }
        LOG_INFO("Number of indices in ") << s + 1 << ". mesh: " << newMesh.VertexIndices.size();
        LOG_INFO("Number of texel indices in ") << s + 1 << ". mesh: " << newMesh.TexIndices.size();
        meshList.push_back(newMesh);
    }
}

} // namespace Renderer
