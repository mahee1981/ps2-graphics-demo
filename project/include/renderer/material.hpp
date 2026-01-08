#ifndef MATERIAL_HPP
#define MATERIAL_HPP

#include <VU0Math/vec4.hpp>

namespace Renderer
{

class Material
{
  public:
    ps2math::Vec4 AmbientColor{0.0f, 0.0f, 0.0f, 1.0f};
    Material(ps2math::Vec4 ambientColor) : AmbientColor(ambientColor) {};
    Material() = default;
};

} // namespace Renderer

#endif
