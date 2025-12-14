#include "renderer/renderer3d.hpp"
#include "utils.hpp"

namespace Renderer
{

Renderer3D::Renderer3D(int width, int height)
    : _screenWidth(width), _screenHeight(height),
      _perspectiveMatrix(
          ps2math::Mat4::perspective(Utils::ToRadians(45.0f), (float)width / (float)height, 0.1f, 2000.0f))
{
}

void Renderer3D::SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far)
{
    _perspectiveMatrix = ps2math::Mat4::perspective(angleRadians, aspectRatio, near, far);
}

} // namespace Renderer
