#include <draw3d.h>
#include <packet2.h>
#include <packet2_types.h>
#include <packet2_utils.h>
#include <packet2_vif.h>
#include <draw.h>

#include <fstream>
#include <memory>

#include "logging/log.hpp"
#include "packet2_chain.h"
#include "renderer/packetedmesh.hpp"

namespace Renderer
{
const std::shared_ptr<packet2_t> RenderChunck::MAX_CHUNCK_HEADER{
    RenderChunck::MakeChunckHeaderPacket(MAX_VERTEXDATA_PER_VIF_PACKET)};

const std::shared_ptr<packet2_t> RenderChunck::MakeChunckHeaderPacket(u32 numOfVertices)
{
    auto bufferHeader = std::shared_ptr<packet2_t>{packet2_create(5, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free};
    packet2_chain_ret(bufferHeader.get(), 0, 0);
    packet2_vif_stcycl(bufferHeader.get(), 0, 0x0101, 0);
    packet2_vif_open_unpack(bufferHeader.get(), P2_UNPACK_V4_32, 0, 1, 0, 1, 0);
    {
        packet2_pad96(bufferHeader.get(), 0);
        packet2_add_u32(bufferHeader.get(), numOfVertices);
        packet2_utils_gs_add_prim_giftag(bufferHeader.get(),
                                         &primitiveTypeConfig,
                                         numOfVertices,
                                         DRAW_STQ2_REGLIST,
                                         3,
                                         0);
    }
    // Append a ret dma tag, since header will be a part of a call chain
    packet2_vif_pad128(bufferHeader.get());
    packet2_chain_close_tag(bufferHeader.get());
    packet2_vif_close_unpack_auto(bufferHeader.get(), 0, 0x0101);

    return bufferHeader;
}

prim_t RenderChunck::primitiveTypeConfig{.type = PRIM_TRIANGLE,
                                         .shading = PRIM_SHADE_GOURAUD,
                                         .mapping = DRAW_ENABLE,
                                         .fogging = DRAW_DISABLE,
                                         .blending = DRAW_ENABLE,
                                         .antialiasing = DRAW_DISABLE,
                                         .mapping_type = PRIM_MAP_ST,
                                         .colorfix = PRIM_UNFIXED};

RenderChunck::RenderChunck(std::size_t numOfVertices)
    : numOfVertices(numOfVertices),
      headerPacket(numOfVertices == MAX_VERTEXDATA_PER_VIF_PACKET ? MAX_CHUNCK_HEADER
                                                                  : MakeChunckHeaderPacket(numOfVertices)),
      vertexData(nullptr), displayList(nullptr)
{
}

// This whole method will be done offline, now just for testing
void RenderChunck::AddVertexData(const std::vector<ps2math::Vec4> &vertices,
                                 const std::vector<ps2math::Vec4> &normals,
                                 const std::vector<ps2math::Vec4> &texels,
                                 std::size_t offset,
                                 std::size_t count)
{
    vertexData =
        std::shared_ptr<packet2_t>(packet2_create(3 * count + 5, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free);
    std::size_t unpackLocation = 2; // because we are sending num of vertices and dma tag first
    packet2_chain_open_cnt(vertexData.get(), 0, 0, 0);
    packet2_vif_stcycl(vertexData.get(), 0, 0x0101, 0);
    packet2_vif_open_unpack(vertexData.get(), P2_UNPACK_V4_32, unpackLocation, 1, 0, 1, 0);
    {
        packet2_add_data(vertexData.get(), (void *)(vertices.data() + offset), count);
        packet2_add_data(vertexData.get(), (void *)(normals.data() + offset), count);
        packet2_add_data(vertexData.get(), (void *)(texels.data() + offset), count);
    }
    packet2_vif_pad128(vertexData.get());
    packet2_chain_close_tag(vertexData.get());
    packet2_vif_close_unpack_auto(vertexData.get(), 0, 0x0101);

    if (offset == 0)
    {
        packet2_chain_ret(vertexData.get(), 0, 0);
        packet2_vif_nop(vertexData.get(), 0);
        packet2_vif_mscal(vertexData.get(), 0, 0);
        packet2_chain_close_tag(vertexData.get());
    }
    else
    {
        packet2_chain_ret(vertexData.get(), 0, 0);
        packet2_vif_nop(vertexData.get(), 0);
        // packet2_vif_pad96(vertexData.get());
        packet2_vif_mscnt(vertexData.get(), 0);
        packet2_chain_close_tag(vertexData.get());
    }
    GenerateDisplayList();
}

void RenderChunck::GenerateDisplayList()
{
    displayList = std::shared_ptr<packet2_t>(packet2_create(5, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free);

    packet2_chain_call(displayList.get(), (const void *)(headerPacket.get()->base), 0, 0, 0);
    packet2_vif_nop(displayList.get(), 0);
    packet2_vif_nop(displayList.get(), 0);
    packet2_chain_close_tag(displayList.get());

    packet2_chain_call(displayList.get(), (const void *)(vertexData.get()->base), 0, 0, 0);
    packet2_vif_nop(displayList.get(), 0);
    packet2_vif_nop(displayList.get(), 0);
    packet2_chain_close_tag(displayList.get());

    packet2_utils_vu_add_end_tag(displayList.get());
}

PacketedMesh::PacketedMesh(const std::vector<ps2math::Vec4> &vertices,
                           const std::vector<ps2math::Vec4> &normals,
                           const std::vector<ps2math::Vec4> &texels)
{
    std::size_t totalVertexCount = vertices.size();
    std::size_t numberOfWholeLoopIterations = totalVertexCount / RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET;
    std::size_t remainder = totalVertexCount % RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET;

    std::size_t offset = 0;
    // TODO: Clean this up, I don't like the convoluted calling
    std::ofstream chunckFile("fileBin.bin", std::ios::binary);
    if (!chunckFile)
    {
        LOG_ERROR("Error opening the file for writing.");
    }

    while (offset < numberOfWholeLoopIterations * RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET)
    {
        renderPackets.push_back(RenderChunck{RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET});
        renderPackets.back().AddVertexData(vertices,
                                           normals,
                                           texels,
                                           offset,
                                           RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET);
        offset += RenderChunck::MAX_VERTEXDATA_PER_VIF_PACKET;
    }
    if (remainder != 0)
    {
        renderPackets.push_back(RenderChunck{remainder});
        renderPackets.back().AddVertexData(vertices, normals, texels, offset, remainder);
    }
}

} // namespace Renderer
