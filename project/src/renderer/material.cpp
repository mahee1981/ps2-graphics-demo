#include "renderer/material.hpp"

namespace Renderer
{
Material::Material(ps2math::Vec4 diffuseColor, std::string texturePath)
    : diffuseColor(diffuseColor), texturePath(texturePath) {};
}
