#ifndef PACKET_MESH_HPP
#define PACKET_MESH_HPP

#include "VU0Math/vec4.hpp"
#include <packet2_types.h>
#include "tiny_obj_loader.h"
#include <debug.h>
#include <draw_primitives.h>
#include <memory>
#include <packet2.h>
#include <vector>

namespace Renderer
{
class RenderChunck
{
  public:
    std::size_t GetNumItems()
    {
        return vertexData != nullptr ? vertexData->base->sw[3] : 0;
    }
    static const std::shared_ptr<packet2_t> MAX_CHUNCK_HEADER;
    static constexpr u32 MAX_VERTEXDATA_PER_VIF_PACKET = 81;

    RenderChunck(std::size_t numOfVertices);
    // Refactor this with strategy pattern
    // Now it's here just for testing
    void AddVertexData(const std::vector<ps2math::Vec4> &vertices,
                       const std::vector<ps2math::Vec4> &normals,
                       const std::vector<ps2math::Vec4> &texels,
                       std::size_t offset,
                       std::size_t count);

    packet2_t *GetDisplayList() const
    {
        return displayList.get();
    }

  private:
    std::size_t numOfVertices;
    std::shared_ptr<packet2_t> headerPacket;
    std::shared_ptr<packet2_t> vertexData;
    std::shared_ptr<packet2_t> displayList;

    void GenerateDisplayList();

    static prim_t primitiveTypeConfig;

    static const std::shared_ptr<packet2_t> MakeChunckHeaderPacket(u32 numOfVertices);
};
class PacketedMesh
{
  public:
    PacketedMesh(const std::vector<ps2math::Vec4> &vertices,
                 const std::vector<ps2math::Vec4> &normals,
                 const std::vector<ps2math::Vec4> &texels);
    const std::vector<RenderChunck> &GetRenderChunks() const
    {
        return renderPackets;
    }

  private:
    std::vector<RenderChunck> renderPackets;
};
} // namespace Renderer
#endif
