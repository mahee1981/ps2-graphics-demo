#ifndef MESH_HPP
#define MESH_HPP

#include "VU0Math/vec4.hpp"
#include "draw_primitives.h"
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
    private:
    std::size_t initialCount;
    public:
    std::vector<ps2math::Vec4> Vertices;
    std::vector<ps2math::Vec4> Normals;
    std::vector<ps2math::Vec4> Texels;
    Mesh(std::size_t count = 0) : initialCount(count), Vertices(count), Normals(count), Texels(count) {} 
    std::size_t GetInitialCount() const { return initialCount; }

};

struct MaterialMesh
{
  private:
    using MaterialIndex = int;
    using NumberOfVertices = std::size_t;
    using MaterialVertMap = std::unordered_map<MaterialIndex, Mesh>;

    MaterialVertMap meshPerMaterialMap;
    std::size_t numVertices;

  public:
    MaterialMesh(const std::unordered_map<MaterialIndex, NumberOfVertices> &materialToFaceCountMap);
    Mesh& operator[](int index);
    const Mesh& operator[](int index) const;
    inline std::size_t GetMaterialCount() const
    {
        return meshPerMaterialMap.size();
    }
    inline std::size_t GetVertexCount() const
    {
        return numVertices;
    }

    MaterialVertMap::iterator begin() { return meshPerMaterialMap.begin(); }
    MaterialVertMap::const_iterator begin() const { return meshPerMaterialMap.begin(); }

    MaterialVertMap::iterator end() { return meshPerMaterialMap.end(); }
    MaterialVertMap::const_iterator end() const { return meshPerMaterialMap.end(); }


};
} // namespace Renderer
#endif
