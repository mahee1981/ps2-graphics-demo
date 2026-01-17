#ifndef IRENDERER_3D
#define IRENDERER_3D

#include "light/BaseLight.hpp"
#include "renderer/model.hpp"
#include <vector>
namespace Renderer
{

class IRenderer3D
{
  protected:
    static constexpr float xOff = 2048.0f;
    static constexpr float yOff = 2048.0f;
    static constexpr u16 MAX_PACKET_SIZE = 0xFFFF;

  public:
    virtual void SetPerspectiveMatrix(float angleRadians, float aspecRation, float near, float far) = 0;
    virtual const ps2math::Mat4 &GetPerspectiveMatrix() const = 0;
    virtual void RenderFrame(const std::vector<Model> &models,
                             const Light::BaseLight &mainLight,
                             const ps2math::Mat4 &viewProjMatrix) = 0;
    virtual void ToggleDebugPrint() = 0;
};

} // namespace Renderer
#endif
