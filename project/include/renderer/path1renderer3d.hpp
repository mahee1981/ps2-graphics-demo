#ifndef PATH1_RENDERER_3D
#define PATH1_RENDERER_3D

#include "interfaces/IRenderer3D.hpp"
#include "tools/Deltawatch.hpp"
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
    void RenderChunck(packet2_t* bufferHeader, const std::size_t vertexCount, ps2math::Mat4 &mvp, ps2math::Mat4 &modelMatrix, const Mesh &mesh, const std::size_t offset, const Light::BaseLight &light);
    packet2_t * staticPacket;
    packet2_t * bufferHeader;
    std::size_t context;
    Deltawatch lastDisplayListPrepWatch;
    static prim_t primitiveTypeConfig;
    void PrepareStaticPacket();
    bool isDebuggingEnabled;
};

} // namespace Renderer

#endif
