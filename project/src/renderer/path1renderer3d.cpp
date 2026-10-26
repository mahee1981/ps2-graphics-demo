#include <memory>
#include <packet2_utils.h>


#include "renderer/path1renderer3d.hpp"
#include "dma.h"
#include "draw.h"

namespace Renderer
{
Path1Renderer3D::Path1Renderer3D(int width, int height, std::shared_ptr<ITextureLoader> textureLoader)
    : VU1Renderer3DBase(width, height),
      passManager(), isStartOfNewChain(true), textureLoader(textureLoader)

{
}

void Path1Renderer3D::RenderChunck(const std::size_t vertexCount,
                                   const Mesh &mesh,
                                   const Material &material,
                                   const u64 texBindSettings,
                                   const std::size_t offset)
{
    packet2_t *currentVifPacket = dynamicPacket[context].get();
    if (isStartOfNewChain == true)
    {
        packet2_reset(currentVifPacket, 0);
    }

    u32 vifAddedBytes = 0;

    AddVertCountGIFTagAndMaterial(currentVifPacket, vertexCount, material, texBindSettings, vifAddedBytes);
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
        AddProgramStartCode(currentVifPacket);
    }
    else
    {
        AddProgramContinueCode(currentVifPacket);
    }

    isStartOfNewChain = false;
}

void Path1Renderer3D::RenderMeshes(unsigned long long &trianglesRendered,
                                   const Model &model,
                                   const u64 modelTextureBindSettings)
{
    for (const auto &materialMesh : model.GetMeshList())
    {
        int numOfEnqueuedChuncks = 0;
        for (const auto &[materialIdx, mesh] : materialMesh)
        {
            const Material &material = model.GetMaterialById(materialIdx);

            std::size_t totalVertexCount = mesh.Vertices.size();
            std::size_t numberOfWholeLoopIterations = totalVertexCount / MAX_VERTEXDATA_PER_VIF_PACKET;
            std::size_t remainder = totalVertexCount % MAX_VERTEXDATA_PER_VIF_PACKET;
            std::size_t offset = 0;

            for (std::size_t i = 0; i < numberOfWholeLoopIterations; i++)
            {
                bool isPacketReadyToFlush = numOfEnqueuedChuncks == 32;
                if (isPacketReadyToFlush)
                {
                    numOfEnqueuedChuncks = 0;
                    SendPacketAndChangeContext(dynamicPacket[context].get());
                }
                RenderChunck(MAX_VERTEXDATA_PER_VIF_PACKET, mesh, material, modelTextureBindSettings, offset);

                offset += MAX_VERTEXDATA_PER_VIF_PACKET;
                trianglesRendered += MAX_VERTEXDATA_PER_VIF_PACKET / 3;
                numOfEnqueuedChuncks++;
            }
            if (remainder != 0)
            {
                RenderChunck(remainder, mesh, material, modelTextureBindSettings, offset);
                numOfEnqueuedChuncks++;
                trianglesRendered += remainder / 3;
            }
        }
        if (numOfEnqueuedChuncks != 0)
            SendPacketAndChangeContext(dynamicPacket[context].get());
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
    passManager.GenerateRenderingPasses(textureLoader->GetTextures(), models);

    passManager.BeginFrame();

    for (u32 passCnt = 0; passCnt < passManager.GetPassCount(); ++passCnt)
    {
        SinglePass currentPass = passManager.GetCurrentPass();
        // TODO: maybe this isn't the best place to wait for the draw_finish
        passManager.WaitFence();

        for (const Model &model : currentPass.GetModelsInPass())
        {
            ps2math::Mat4 modelMatrix = model.GetWorldMatrix();
            ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;
            // TODO: move texture out of model and look up per material
            const u64 modelTextureBindData = currentPass.GetModelTextureBindSettings(model);
            SendDynamicModelData(mvp, modelMatrix, mainLight, model.GetTextureById(0), cameraPos);
            RenderMeshes(trianglesRendered, model, modelTextureBindData);
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

void Path1Renderer3D::AddVertCountGIFTagAndMaterial(packet2_t *vu1Packet,
                                                    u32 numOfVertices,
                                                    const Material &material,
                                                    const u64 texBindSettings,
                                                    u32 &vifAddedBytes)
{
    packet2_utils_vu_open_unpack(vu1Packet, vifAddedBytes, 1);
    {
        auto &diffuseColor = material.GetDiffuseColor();
        packet2_add_float(vu1Packet, 128 * diffuseColor.r);
        packet2_add_float(vu1Packet, 128 * diffuseColor.g);
        packet2_add_float(vu1Packet, 128 * diffuseColor.b);
        packet2_add_u32(vu1Packet, numOfVertices);
        packet2_utils_gs_add_prim_giftag(vu1Packet,
                                         &primitiveTypeConfig,
                                         numOfVertices,
                                         DRAW_STQ2_REGLIST,
                                         3,
                                         0);

        qword_t textureConfig;
        textureConfig.dw[0] = texBindSettings;
        textureConfig.dw[1] = GS_REG_TEX0_1;
        packet2_add_u128(vu1Packet, textureConfig.qw);
    }
    packet2_utils_vu_close_unpack(vu1Packet); 
    vifAddedBytes += 2;
}

void Path1Renderer3D::AddProgramStartCode(packet2_t *vu1Packet)
{
    packet2_chain_open_cnt(vu1Packet, 0, 0, 0);
    packet2_vif_nop(vu1Packet, 0);
    packet2_vif_mscal(vu1Packet, 0, 0);
    packet2_chain_close_tag(vu1Packet);
}

void Path1Renderer3D::AddProgramContinueCode(packet2_t *vu1Packet)
{
    packet2_chain_open_cnt(vu1Packet, 0, 0, 0);
    packet2_vif_nop(vu1Packet, 0);
    packet2_vif_mscnt(vu1Packet, 0);
    packet2_chain_close_tag(vu1Packet);
}
void Path1Renderer3D::SendPacketAndChangeContext(packet2_t *currentVifPacket)
{
    isStartOfNewChain = true;
    packet2_utils_vu_add_end_tag(currentVifPacket);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);
    // Switch packet, so we can proceed during DMA transfer
    context = !context;
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
