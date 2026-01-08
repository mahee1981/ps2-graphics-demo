#ifndef MESH_HPP
#define MESH_HPP

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "tiny_obj_loader.h"
#include <debug.h>
#include <draw_types.h>
#include <vector>
namespace Renderer
{
class Mesh
{
  public:
    std::vector<ps2math::Vec4> Vertices;
    std::vector<ps2math::Vec4> Normals;
    std::vector<texel_t> Texels;
};
} // namespace Renderer
#endif
