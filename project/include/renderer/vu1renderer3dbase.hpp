#ifndef VU1_RENDERER_3D_BASE
#define VU1_RENDERER_3D_BASE

#include "VU0Math/vec4.hpp"
#include "graphics/Texture.hpp"
#include "interfaces/IRenderer3D.hpp"
#include "tools/Deltawatch.hpp"

#include <array>
#include <memory>
#include <sstream>

namespace Renderer
{
class VU1Renderer3DBase : public IRenderer3D
{
  public:
    VU1Renderer3DBase(int width, int height);
    virtual ~VU1Renderer3DBase() = default;
    void SetPerspectiveMatrix(float angleRadians, float aspecRation, float near, float far) override;
    const ps2math::Mat4 &GetPerspectiveMatrix() const override
    {
        return _perspectiveMatrix;
    }
    void RenderFrame(const std::vector<Model> &models,
                     const Light::BaseLight &mainLight,
                     const ps2math::Mat4 &viewProjMatrix,
                     const ps2math::Vec4 &cameraPos) override;
    void ToggleDebugPrint() override;
    void UploadVU1MicroProgram(u32 *VU1Draw3D_CodeStart, u32 *VU1Draw3D_CodeEnd);
    void SetDoubleBufferSettings();

  protected:
    using SmartPacket = std::unique_ptr<packet2_t, decltype(&packet2_free)>;

    float _screenWidth;
    float _screenHeight;
    ps2math::Mat4 _perspectiveMatrix;
    alignas(64) std::array<SmartPacket, 2> dynamicPacket;
    SmartPacket staticScaleAndColorPacket;
    std::size_t context;
    Deltawatch lastDisplayListPrepWatch;
    bool isDebuggingEnabled;

    void SendDynamicModelData(const ps2math::Mat4 &mvp,
                              const ps2math::Mat4 &modelMatrix,
                              const Light::BaseLight &light,
                              const ps2math::Vec4 &cameraPos);
    void RenderChunck(packet2_t *bufferHeader,
                      const std::size_t vertexCount,
                      const Mesh &mesh,
                      const std::size_t offset);
    void PrepareScaleAndColorPacket();
    virtual void DoRenderFrame(const std::vector<Model> &models,
                               const Light::BaseLight &mainLight,
                               const ps2math::Mat4 &viewProjMatrix,
                               const ps2math::Vec4 &cameraPos) = 0;
    inline void AppendScreenPrintLine(const std::string &line)
    {
        logStream << line << "\n";
    }

  private:
    void PrintDebugInfo();
    std::stringstream logStream;
    char processTimeMessageBuff[70];
};

} // namespace Renderer

#endif
