#include <memory>
#include <sstream>
#include <fstream>
#include "mesh/model.hpp"

void Model::LoadModel(const char *fileName, const char *material_search_path )
{
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = material_search_path; // Path to material files

    tinyobj::ObjReader reader;

	if(!reader.ParseFromFile(fileName, reader_config))
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
    printf("Number of shapes: %zu\n", shapes.size());
    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        Mesh newMesh;
        newMesh.VertexPositionCoord.reserve(attrib.vertices.size() /3);
        for (size_t vi = 0; vi < attrib.vertices.size() / 3; ++vi)
        {
            tinyobj::real_t vx = attrib.vertices[3 * vi + 0];
            tinyobj::real_t vy = attrib.vertices[3 * vi + 1];
            tinyobj::real_t vz = attrib.vertices[3 * vi + 2];
            newMesh.VertexPositionCoord.emplace_back(vx, vy, vz, 1.0f);
        }
		
		newMesh.TexCoordinates.reserve(attrib.texcoords.size() / 2);
        for (size_t vi = 0; vi < attrib.texcoords.size() / 2; ++vi)
        {
            texel_t texel;
            texel.u = attrib.texcoords[2 * vi + 0];
            texel.v = attrib.texcoords[2 * vi + 1];
            newMesh.TexCoordinates.push_back(texel);
        }

		newMesh.TexIndices.reserve(shapes[s].mesh.indices.size());
		newMesh.VertexIndices.reserve(shapes[s].mesh.indices.size());

        for(const auto &idx : shapes[s].mesh.indices)
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
        printf("Number of vertexes: %zu\n", attrib.vertices.size());
        printf("Number of texels:  %zu \n", attrib.texcoords.size());
        meshList.push_back(newMesh);
    }
}
