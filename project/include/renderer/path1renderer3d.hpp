#ifndef PATH1_RENDERER_3D
#define PATH1_RENDERER_3D

#include "VU0Math/vec4.hpp"
#include "renderer/PassManager.hpp"
#include "renderer/vu1renderer3dbase.hpp"

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
    // must be divisble by 3 so you avoid atrifacts between batches dumass
    static constexpr u32 MAX_VERTEXDATA_PER_VIF_PACKET = 81;
    static prim_t primitiveTypeConfig;

    char trianglesRenderedMessageBuff[70];
    PassManager passManager;
    bool isStartOfNewChain;

    void RenderChunck(const std::size_t vertexCount, const Mesh &mesh, const Material &material, std::size_t offset);
    // Adds vertex count and GIF tag to outgoing VU1 packet
    inline void AddVertCountGIFTagAndMaterial(packet2_t *vu1Packet, u32 numOfVertices, const Material &material, u32 &vifAddedBytes);
    inline void AddProgramStartCode(packet2_t *vu1Packet);
    inline void AddProgramContinueCode(packet2_t *vu1Packet);
    inline void SendPacketAndChangeContext(packet2_t *vu1Packet);
    void RenderMeshes(unsigned long long &trianglesRendered, const Model &model);
};

} // namespace Renderer

#endif
