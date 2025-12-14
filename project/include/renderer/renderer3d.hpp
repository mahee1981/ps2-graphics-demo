#ifndef RENDERER_3D_HPP
#define RENDERER_3D_HPP

#include "VU0Math/mat4.hpp"

namespace Renderer
{

class Renderer3D
{
  public:
    Renderer3D(int width, int height);
    void SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far);
    const ps2math::Mat4 &GetPerspectiveMatrix() const
    {
        return _perspectiveMatrix;
    }

  private:
    float _screenWidth;
    float _screenHeight;
    ps2math::Mat4 _perspectiveMatrix;
};

} // namespace Renderer

#endif
