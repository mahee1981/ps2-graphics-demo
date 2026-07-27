#ifndef TRISTRIP_RENDERER_3D
#define TRISTRIP_RENDERER_3D

#include "VU0Math/vec4.hpp"
#include "renderer/vu1renderer3dbase.hpp"

namespace Renderer
{
// NOT READY
class TriangleStripRenderer3D : public VU1Renderer3DBase
{
  public:
    TriangleStripRenderer3D(int width, int height);
    ~TriangleStripRenderer3D() = default;
    const ps2math::Mat4 &GetPerspectiveMatrix() const override
    {
        return _perspectiveMatrix;
    }
    void DoRenderFrame(const std::vector<Model> &models,
                       const Light::BaseLight &mainLight,
                       const ps2math::Mat4 &viewProjMatrix,
                       const ps2math::Vec4 &cameraPos) override;

  private:
    static prim_t primitiveTypeConfig;
    SmartPacket bufferHeader;
    char trianglesRenderedMessageBuff[70];
    void RenderChunck(packet2_t *bufferHeader,
                      const std::size_t vertexCount,
                      const Mesh &mesh,
                      const std::size_t offset);
};

} // namespace Renderer

#endif
