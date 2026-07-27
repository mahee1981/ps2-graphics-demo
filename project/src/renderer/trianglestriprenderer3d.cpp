#include "renderer/trianglestriprenderer3d.hpp"
#include "VU0Math/vec4.hpp"
#include "graphics/Texture.hpp"
#include "logging/log.hpp"
#include "packet2.h"
#include "packet2_utils.h"
#include "packet2_vif.h"
#include "renderer/vu1renderer3dbase.hpp"
#include "utils.hpp"
#include <cstdio>
#include <memory>

namespace Renderer
{
TriangleStripRenderer3D::TriangleStripRenderer3D(int width, int height)
    : VU1Renderer3DBase(width, height),
      bufferHeader{SmartPacket{packet2_create(30, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free}}


{
}

void TriangleStripRenderer3D::RenderChunck(packet2_t *header,
                                   const std::size_t vertexCount,
                                   const Mesh &mesh,
                                   const std::size_t offset)
{
    packet2_t *currentVifPacket = dynamicPacket[context].get();
    packet2_reset(currentVifPacket, 0);

    u32 vifAddedBytes = 0;

    packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, header->base, packet2_get_qw_count(header), 1);
    vifAddedBytes += packet2_get_qw_count(header);
    // Add vertices
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes,
                                     (void *)(mesh.Vertices.data() + offset),
                                     vertexCount,
                                     1);
    vifAddedBytes += vertexCount; // one VECTOR is size of qword

    // Add normals
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes,
                                     (void *)(mesh.Normals.data() + offset),
                                     vertexCount,
                                     1);
    vifAddedBytes += vertexCount;
    // Add sts
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes,
                                     (void *)(mesh.Texels.data() + offset),
                                     vertexCount,
                                     1);
    vifAddedBytes += vertexCount;
    if (offset == 0)
    {
        packet2_chain_open_cnt(currentVifPacket, 0, 0, 0);
        packet2_vif_pad96(currentVifPacket);
        packet2_vif_mscal(currentVifPacket, 0, 0);
        packet2_chain_close_tag(currentVifPacket);
    }
    else
    {
        packet2_chain_open_cnt(currentVifPacket, 0, 0, 0);
        packet2_vif_pad96(currentVifPacket);
        packet2_vif_mscnt(currentVifPacket, 0);
        packet2_chain_close_tag(currentVifPacket);
    }

    packet2_utils_vu_add_end_tag(currentVifPacket);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);

    // Switch packet, so we can proceed during DMA transfer
    context = !context;
}
void TriangleStripRenderer3D::DoRenderFrame(const std::vector<Model> &models,
                                  const Light::BaseLight &mainLight,
                                  const ps2math::Mat4 &viewMat,
                                  const ps2math::Vec4 &cameraPos)
{
    unsigned long long trianglesRendered = 0;
    const auto viewProjMat = viewMat * _perspectiveMatrix;
    for (const Model &model : models)
    {
        ps2math::Mat4 modelMatrix = model.GetWorldMatrix();
        ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;
        SendDynamicModelData(mvp, modelMatrix, mainLight, model.GetTextureById(0), cameraPos);

        for (const auto &packetedMesh : model.GetPacketedMeshList())
        {
            for (const auto &renderChunck : packetedMesh.GetRenderChunks())
            {
                packet2_t* displayList = renderChunck.GetDisplayList();
                dma_channel_wait(DMA_CHANNEL_VIF1, 0);
                dma_channel_wait(DMA_CHANNEL_GIF, 0);
                dma_channel_send_packet2(displayList, DMA_CHANNEL_VIF1, 1);
            }
        }

    }
    if(isDebuggingEnabled)
    {
        std::sprintf(trianglesRenderedMessageBuff, "Triangles rendered: %lld", trianglesRendered);
        AppendScreenPrintLine(trianglesRenderedMessageBuff);
    }
}
prim_t TriangleStripRenderer3D::primitiveTypeConfig{.type = PRIM_TRIANGLE,
                                            .shading = PRIM_SHADE_GOURAUD,
                                            .mapping = DRAW_ENABLE,
                                            .fogging = DRAW_DISABLE,
                                            .blending = DRAW_ENABLE,
                                            .antialiasing = DRAW_DISABLE,
                                            .mapping_type = PRIM_MAP_ST,
                                            .colorfix = PRIM_UNFIXED};

} // namespace Renderer
