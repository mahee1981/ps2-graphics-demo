#ifndef MESH_HPP
#define MESH_HPP

#include "VU0Math/vec4.hpp"
#include "tiny_obj_loader.h"
#include <cstddef>
#include <debug.h>
#include <draw_types.h>
#include <packet2.h>
#include <unordered_map>
#include <vector>
namespace Renderer
{
class Mesh
{
  public:
    std::vector<ps2math::Vec4> Vertices;
    std::vector<ps2math::Vec4> Normals;
    std::vector<ps2math::Vec4> Texels;
    Mesh(std::size_t count = 0) : Vertices(count), Normals(count), Texels(count), initialCount(count)
    {
    }
    std::size_t GetInitialCount() const
    {
        return initialCount;
    }

  private:
    std::size_t initialCount;
};

class MaterialMesh
{
    using MaterialIndex = int;
    using NumberOfVertices = std::size_t;
    using MeshMaterialMap = std::unordered_map<MaterialIndex, Mesh>;

  public:
    MaterialMesh(const std::unordered_map<MaterialIndex, NumberOfVertices> &materialToFaceCountMap);
    Mesh &operator[](int index);
    const Mesh &operator[](int index) const;
    inline std::size_t GetMaterialCount() const
    {
        return meshPerMaterialMap.size();
    }
    inline std::size_t GetVertexCount() const
    {
        return numVertices;
    }

    MeshMaterialMap::iterator begin()
    {
        return meshPerMaterialMap.begin();
    }
    MeshMaterialMap::const_iterator begin() const
    {
        return meshPerMaterialMap.begin();
    }

    MeshMaterialMap::iterator end()
    {
        return meshPerMaterialMap.end();
    }
    MeshMaterialMap::const_iterator end() const
    {
        return meshPerMaterialMap.end();
    }

  private:
    MeshMaterialMap meshPerMaterialMap;
    std::size_t numVertices;
};
} // namespace Renderer
#endif
