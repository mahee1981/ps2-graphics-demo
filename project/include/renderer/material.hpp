#ifndef MATERIAL_HPP
#define MATERIAL_HPP

#include <VU0Math/vec4.hpp>
#include <string>

namespace Renderer
{

class Material
{
    ps2math::Vec4 diffuseColor{0.0f, 0.0f, 0.0f, 1.0f};
    std::string texturePath;

  public:
    Material(ps2math::Vec4 diffuseColor, std::string texturePath);
    Material() = default;

    inline const std::string &GetTexturePath() const
    {
        return texturePath;
    }
    inline const ps2math::Vec4 &GetDiffuseColor() const
    {
        return diffuseColor;
    }
};

} // namespace Renderer

#endif
