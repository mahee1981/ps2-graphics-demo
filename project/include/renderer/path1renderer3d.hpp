#ifndef PATH1_RENDERER_3D
#define PATH1_RENDERER_3D

#include "interfaces/IRenderer3D.hpp"
#include "utils.hpp"

namespace Renderer
{
class Path1Renderer3D : public IRenderer3D
{
  public:
    Path1Renderer3D(int width, int height);
    ~Path1Renderer3D();
    void SetPerspectiveMatrix(float angleRadians, float aspecRation, float near, float far) override;
    const ps2math::Mat4 &GetPerspectiveMatrix() const override
    {
        return _perspectiveMatrix;
    }
    void RenderFrame(const std::vector<Model> &models,
                     const Light::BaseLight &mainLight,
                     const ps2math::Mat4 &viewProjMatrix) override;
    void ToggleDebugPrint() override;
    void UploadVU1MicroProgram(u32 *VU1Draw3D_CodeStart, u32 *VU1Draw3D_CodeEnd);
    void SetDoubleBufferSettings();
    

  private:
    float _screenWidth;
    float _screenHeight;
    ps2math::Mat4 _perspectiveMatrix;
    alignas(64) packet2_t * dynamicPacket[2];

    packet2_t * staticPacket;
    std::size_t context;
    static prim_t primitiveTypeConfig;
};

} // namespace Renderer

#endif
