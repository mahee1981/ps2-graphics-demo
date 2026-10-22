#include <packet2_utils.h>

#include <memory>

#include "renderer/path1renderer3d.hpp"
#include "dma.h"
#include "draw.h"
#include "logging/log.hpp"

namespace Renderer
{
Path1Renderer3D::Path1Renderer3D(int width, int height)
    : VU1Renderer3DBase(width, height),
      bufferHeader{SmartPacket{packet2_create(10, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free}},
      passManager()

{
}

void Path1Renderer3D::RenderChunck(packet2_t *header,
                                   const std::size_t vertexCount,
                                   const Mesh &mesh,
                                   const std::size_t offset,
                                   bool isPacketReadyToFlush)
{
    static bool isPacketFlushed = true;
    packet2_t *currentVifPacket = dynamicPacket[context].get();
    if (isPacketFlushed == true)
    {
        packet2_reset(currentVifPacket, 0);
    }

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
        packet2_vif_nop(currentVifPacket, 0);
        packet2_vif_mscal(currentVifPacket, 0, 0);
        packet2_chain_close_tag(currentVifPacket);
    }
    else
    {
        packet2_chain_open_cnt(currentVifPacket, 0, 0, 0);
        packet2_vif_nop(currentVifPacket, 0);
        packet2_vif_mscnt(currentVifPacket, 0);
        packet2_chain_close_tag(currentVifPacket);
    }
    isPacketFlushed = false;
    if (isPacketReadyToFlush)
    {
        isPacketFlushed = true;
        packet2_utils_vu_add_end_tag(currentVifPacket);
        dma_channel_wait(DMA_CHANNEL_VIF1, 0);
        dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);
        // Switch packet, so we can proceed during DMA transfer
        context = !context;
    }
}
void Path1Renderer3D::DoRenderFrame(const std::vector<Model> &models,
                                    const Light::BaseLight &mainLight,
                                    const ps2math::Mat4 &viewMat,
                                    const ps2math::Vec4 &cameraPos)
{
    unsigned long long trianglesRendered = 0;
    const auto viewProjMat = viewMat * _perspectiveMatrix;

    // generate rendering passes
    // Begin Frame(Prefetch pass 1 textures)
    // for each pass
    // wait on path3
    // kick off path 1
    // kick off path 3
    // repeat
    passManager.GenerateRenderingPasses(models);

    passManager.BeginFrame();

    for (u32 passCnt = 0; passCnt < passManager.GetPassCount(); ++passCnt)
    {

        SinglePass currentPass = passManager.GetCurrentPass();
        passManager.WaitFence();

        for (const Model &model : currentPass.models)
        {
            ps2math::Mat4 modelMatrix = model.GetWorldMatrix();
            ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;
            SendDynamicModelData(mvp, modelMatrix, mainLight, model.GetTextureById(0), cameraPos);
            for (const auto &materialMesh : model.GetMeshList())
            {
                for (const auto &kv : materialMesh)
                {
                    // TODO: Move packet sending logic out of loop
                    Mesh mesh = kv.second;
                    // LOG_INFO("Material mesh size: ") << mesh.Vertices.size();

                    std::size_t totalVertexCount = mesh.Vertices.size();
                    
                    std::size_t numberOfWholeLoopIterations = totalVertexCount / MAX_VERTEXDATA_PER_VIF_PACKET;
                    std::size_t remainder = totalVertexCount % MAX_VERTEXDATA_PER_VIF_PACKET;
                    std::size_t offset = 0;
                    int numOfEnqueuedChuncks = 0;

                    for (std::size_t i = 0; i < numberOfWholeLoopIterations; i++)
                    {
                        bool isPacketReadyToFlush = i == numberOfWholeLoopIterations - 1 || numOfEnqueuedChuncks == 32;
                        if (isPacketReadyToFlush)
                        {
                            numOfEnqueuedChuncks = 0;
                        }
                        RenderChunck(MAX_CHUNCK_HEADER.get(),
                                     MAX_VERTEXDATA_PER_VIF_PACKET,
                                     mesh,
                                     offset,
                                     isPacketReadyToFlush);

                        offset += MAX_VERTEXDATA_PER_VIF_PACKET;
                        trianglesRendered += MAX_VERTEXDATA_PER_VIF_PACKET / 3;
                        numOfEnqueuedChuncks++;
                    }
                    if (remainder != 0)
                    {
                        packet2_reset(bufferHeader.get(), 0);
                        packet2_pad96(bufferHeader.get(), 0);
                        packet2_add_u32(bufferHeader.get(), remainder);
                        packet2_utils_gs_add_prim_giftag(bufferHeader.get(),
                                                         &primitiveTypeConfig,
                                                         remainder,
                                                         DRAW_STQ2_REGLIST,
                                                         3,
                                                         0);
                        RenderChunck(bufferHeader.get(), remainder, mesh, offset, true);

                        trianglesRendered += remainder / 3;
                    }
                }
            }
        }

        passManager.PrefetchNextPassTextures();
        passManager.EndPass();
        // passManager.DebugDump();
    }
    if (isDebuggingEnabled)
    {
        std::sprintf(trianglesRenderedMessageBuff, "Triangles rendered: %lld", trianglesRendered);
        AppendScreenPrintLine(trianglesRenderedMessageBuff);
    }
}

const std::shared_ptr<packet2_t> Path1Renderer3D::MAX_CHUNCK_HEADER{Path1Renderer3D::MakeChunckHeaderPacket(MAX_VERTEXDATA_PER_VIF_PACKET)};

const std::shared_ptr<packet2_t> Path1Renderer3D::MakeChunckHeaderPacket(u32 numOfVertices)
{
    std::shared_ptr<packet2_t> bufferHeader =
        std::shared_ptr<packet2_t>(packet2_create(5, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free);

    packet2_pad96(bufferHeader.get(), 0);
    packet2_add_u32(bufferHeader.get(), numOfVertices);
    packet2_utils_gs_add_prim_giftag(bufferHeader.get(), &primitiveTypeConfig, numOfVertices, DRAW_STQ2_REGLIST, 3, 0);

    return bufferHeader;
}

prim_t Path1Renderer3D::primitiveTypeConfig{.type = PRIM_TRIANGLE,
                                            .shading = PRIM_SHADE_GOURAUD,
                                            .mapping = DRAW_ENABLE,
                                            .fogging = DRAW_DISABLE,
                                            .blending = DRAW_ENABLE,
                                            .antialiasing = DRAW_DISABLE,
                                            .mapping_type = PRIM_MAP_ST,
                                            .colorfix = PRIM_UNFIXED};
} // namespace Renderer
