#include "mesh/model.hpp"

void Model::LoadModel(const char *fileName)
{

    tinyobj::ObjReaderConfig reader_config;

    tinyobj::ObjReader reader;

    std::ifstream mainFile(fileName);
    std::stringstream buffer;
    buffer << mainFile.rdbuf();

    std::ifstream materialFile("cdrom0:\\CAT\\MESH_CAT.MTL;1");
    std::stringstream materiaLbuffer;
    materiaLbuffer << materialFile.rdbuf();

    if (!reader.ParseFromString(buffer.str(), materiaLbuffer.str(), reader_config))
    {
        if (!reader.Error().empty())
        {

            printf("TinyObjReader: %s", reader.Error().c_str());
        }
    }

    if (!reader.Warning().empty())
    {
        printf("TinyObjReader: %s", reader.Warning().c_str());
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    // auto &materials = reader.GetMaterials();
    printf("Number of shapes: %d\n", shapes.size());
    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        Mesh newMesh;
        newMesh._vertexPositionCoord.resize(attrib.vertices.size() /3);
        for (size_t vi = 0; vi < attrib.vertices.size() / 3; ++vi)
        {
            tinyobj::real_t vx = attrib.vertices[3 * vi + 0];
            tinyobj::real_t vy = attrib.vertices[3 * vi + 1];
            tinyobj::real_t vz = attrib.vertices[3 * vi + 2];
            newMesh._vertexPositionCoord[vi] = ps2math::Vec4(vx, vy, vz, 1.0f);
        }

        for(const auto &idx : shapes[s].mesh.indices)
        {
            newMesh._vertexIndices.push_back(idx.vertex_index);
        }
        meshList.push_back(newMesh);
    }
}