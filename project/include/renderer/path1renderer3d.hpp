#ifndef PATH1_RENDERER_3D
#define PATH1_RENDERER_3D

#include "VU0Math/vec4.hpp"
#include "renderer/vu1renderer3dbase.hpp"
#include "graphics/TextureCache.hpp"
#include <memory>

namespace Renderer
{
class Path1Renderer3D : public VU1Renderer3DBase
{
  public:
    Path1Renderer3D(int width, int height);
    ~Path1Renderer3D() = default;
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
    static const std::shared_ptr<packet2_t> MAX_CHUNCK_HEADER;
    static const std::shared_ptr<packet2_t> MakeChunckHeaderPacket(u32 numOfVertices);

    SmartPacket bufferHeader;
    char trianglesRenderedMessageBuff[70];
    TextureCache textureCache;

    void RenderChunck(packet2_t *bufferHeader,
                      const std::size_t vertexCount,
                      const Mesh &mesh,
                      const std::size_t offset,
                      bool isTimeToFlush);
};

} // namespace Renderer

#endif
