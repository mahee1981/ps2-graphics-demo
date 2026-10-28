#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

#include "renderer/mesh.hpp"

namespace Renderer
{

MaterialMesh::MaterialMesh(const std::unordered_map<MaterialIndex, NumberOfVertices> &materialToVertexCountMap)
{
    meshPerMaterialMap.reserve(materialToVertexCountMap.size());
    std::transform(materialToVertexCountMap.begin(),
                   materialToVertexCountMap.end(),
                   std::inserter(meshPerMaterialMap, meshPerMaterialMap.end()),
                   [](const auto &kv) { return std::make_pair(kv.first, Mesh(kv.second)); });
    numVertices =
        std::accumulate(materialToVertexCountMap.begin(),
                        materialToVertexCountMap.end(),
                        0,
                        [](int sum, const std::pair<MaterialIndex, NumberOfVertices> &kv) {
                            return sum + kv.second;
                        });
}

Mesh& MaterialMesh::operator[](int index)
{
    return meshPerMaterialMap[index];
}

const Mesh& MaterialMesh::operator[](int index) const
{
    return meshPerMaterialMap.at(index);
}

} // namespace Renderer
