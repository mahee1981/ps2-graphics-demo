#ifndef MESH_HPP
#define MESH_HPP

#include "tiny_obj_loader.h"
#include <debug.h>
#include <draw_types.h>
#include <vector>
namespace Renderer
{
class Mesh
{
  public:
    std::vector<int> VertexIndices;
    std::vector<int> NormalIndices;
    std::vector<int> TexIndices;
};
} // namespace Renderer
#endif
