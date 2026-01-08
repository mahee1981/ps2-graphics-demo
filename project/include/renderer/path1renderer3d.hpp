#ifndef PATH1_RENDERER_3D
#define PATH1_RENDERER_3D

#include "interfaces/IRenderer3D.hpp"

namespace Renderer
{
class Path1Renderer3D : public IRenderer3D
{
  public:
    Path1Renderer3D(int width, int height);
    void SetPerspectiveMatrix(float angleRadians, float aspecRation, float near, float far) override;
    const ps2math::Mat4 &GetPerspectiveMatrix() const override;
    void RenderFrame(const std::vector<Model> &models,
                     const Light::BaseLight &mainLight,
                     const ps2math::Mat4 &viewProjMatrix) override;
    void ToggleDebugPrint() override;

  private:
    float _screenWidth;
    float _screenHeight;
    ps2math::Mat4 _perspectiveMatrix;
};

} // namespace Renderer

#endif
