#include "renderer/path1renderer3d.hpp"
#include "logging/log.hpp"
#include "packet2_utils.h"
#include "packet2_vif.h"
#include "utils.hpp"

namespace Renderer
{
Path1Renderer3D::Path1Renderer3D(int width, int height)
    : _screenWidth(width), _screenHeight(height),
      _perspectiveMatrix(
          ps2math::Mat4::perspective(Utils::ToRadians(60.0f), (float)width / (float)height, 1.0f, 2000.0f))

{
    dynamicPacket[0] = packet2_create(30, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    dynamicPacket[1] = packet2_create(30, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    staticPacket = packet2_create(10, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    bufferHeader = packet2_create(10, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);

    _perspectiveMatrix = ps2math::Mat4::SpecializePerspectiveForVU1(_perspectiveMatrix, width, height);
    PrepareStaticPacket();
}
void Path1Renderer3D::SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far)
{
    _perspectiveMatrix = ps2math::Mat4::perspective(angleRadians, aspectRatio, near, far);
}
void Path1Renderer3D::UploadVU1MicroProgram(u32 *VU1Draw3D_CodeStart, u32 *VU1Draw3D_CodeEnd)
{
    u32 packet_size =
        packet2_utils_get_packet_size_for_program(VU1Draw3D_CodeStart, VU1Draw3D_CodeEnd) + 1; // + 1 for end tag
    packet2_t *packet2 = packet2_create(packet_size, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    packet2_vif_add_micro_program(packet2, 0, VU1Draw3D_CodeStart, VU1Draw3D_CodeEnd);
    packet2_utils_vu_add_end_tag(packet2);
    dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    packet2_free(packet2);
}

void Path1Renderer3D::PrepareStaticPacket()
{
    packet2_add_float(staticPacket, 2048.0F);  // scale
    packet2_add_float(staticPacket, 2048.0F);  // scale
    float zScale = float(0xFFFFFFFFu) / 32.0F;
    LOG_INFO("z-scale: ") << zScale;
    packet2_add_float(staticPacket, zScale); // scale
    packet2_add_float(staticPacket, 1.0f);   // scale
    u8 j = 0;                                // RGBA
    for (j = 0; j < 4; j++)
        packet2_add_u32(staticPacket, 128);
}

void Path1Renderer3D::RenderChunck(packet2_t *header,
                                   const std::size_t vertexCount,
                                   ps2math::Mat4 &mvp,
                                   const Mesh &mesh,
                                   const std::size_t offset)
{
    packet2_t *currentVifPacket = dynamicPacket[context];
    packet2_reset(currentVifPacket, 0);

    // This is still a static part of the packet, but handling it this way writes the memory many times
    // Add the matrix at the top of the memory via REF tag so no copy here and skip the TOP register shenaningans
    // TODO: try sending this only once
    u32 vifAddedBytes = 0; // zero because now we will use TOP register (double buffer)
    packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, mvp.GetDataPtr(), 4, 0);
    vifAddedBytes += 4;

    // Merge packets
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes, // because of the world Matrix
                                     staticPacket->base,
                                     packet2_get_qw_count(staticPacket),
                                     0);
    vifAddedBytes += packet2_get_qw_count(staticPacket);

    vifAddedBytes = 0; // zero because now we will use TOP register (double buffer)
                       // we don't wan't to unpack at 8 + beggining of buffer, but at
                       // the beggining of the buffer

    // Add Vertex Count And GifTag
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes, // because of the world Matrix
                                     header->base,
                                     packet2_get_qw_count(header),
                                     1);
    vifAddedBytes += packet2_get_qw_count(header);
    // Add vertices
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes,
                                     (void *)(mesh.Vertices.data() + offset),
                                     vertexCount,
                                     1);
    vifAddedBytes += vertexCount; // one VECTOR is size of qword

    // Add sts
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes,
                                     (void *)(mesh.Texels.data() + offset),
                                     vertexCount,
                                     1);
    vifAddedBytes += vertexCount;
    if (offset == 0)
        packet2_utils_vu_add_start_program(currentVifPacket, 0);
    else
        packet2_utils_vu_add_continue_program(currentVifPacket);
    packet2_utils_vu_add_end_tag(currentVifPacket);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);

    // Switch packet, so we can proceed during DMA transfer
    context = !context;
}
void Path1Renderer3D::RenderFrame(const std::vector<Model> &models,
                                  const Light::BaseLight &mainLight,
                                  const ps2math::Mat4 &viewMat)
{
    const auto viewProjMat = viewMat * _perspectiveMatrix;
    // must be divisble by 3 so you avoid atrifacts between batches dumass
    constexpr u32 MAX_VERTEXDATA_PER_VIF_PACKET = 99;
    for (const Model &model : models)
    {
        ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;

        for (const auto &mesh : model.GetMeshList())
        {
            std::size_t totalVertexCount = mesh.Vertices.size();
            std::size_t numberOfWholeLoopIterations = totalVertexCount / MAX_VERTEXDATA_PER_VIF_PACKET;
            std::size_t remainder = totalVertexCount % MAX_VERTEXDATA_PER_VIF_PACKET;
            packet2_pad96(bufferHeader, 0);
            packet2_add_u32(bufferHeader, MAX_VERTEXDATA_PER_VIF_PACKET);
            packet2_utils_gs_add_prim_giftag(bufferHeader,
                                             &primitiveTypeConfig,
                                             MAX_VERTEXDATA_PER_VIF_PACKET,
                                             DRAW_STQ2_REGLIST,
                                             3,
                                             0);

            std::size_t offset = 0;
            // TODO: Clean this up, I don't like the convoluted calling
            while (offset < numberOfWholeLoopIterations * MAX_VERTEXDATA_PER_VIF_PACKET)
            {
                RenderChunck(bufferHeader, MAX_VERTEXDATA_PER_VIF_PACKET, mvp, mesh, offset);
                offset += MAX_VERTEXDATA_PER_VIF_PACKET;
            }
            if (remainder != 0)
            {
                packet2_reset(bufferHeader, 0);
                packet2_add_u32(bufferHeader, remainder);
                packet2_add_u32(bufferHeader, remainder);
                packet2_add_u32(bufferHeader, remainder);
                packet2_add_u32(bufferHeader, remainder);
                packet2_utils_gs_add_prim_giftag(bufferHeader,
                                                 &primitiveTypeConfig,
                                                 remainder,
                                                 DRAW_STQ2_REGLIST,
                                                 3,
                                                 0);
                RenderChunck(bufferHeader, remainder, mvp, mesh, offset);
            }
            packet2_reset(bufferHeader, 0);
        }
    }
    graph_wait_vsync();
}
void Path1Renderer3D::SetDoubleBufferSettings()
{
    packet2_t *packet2 = packet2_create(1, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    packet2_utils_vu_add_double_buffer(packet2, 6, (1024 - 6) / 2);
    packet2_utils_vu_add_end_tag(packet2);
    dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    packet2_free(packet2);
}

void Path1Renderer3D::ToggleDebugPrint()
{
}

Path1Renderer3D::~Path1Renderer3D()
{
    for (auto &packet : dynamicPacket)
    {
        packet2_free(packet);
    }
    packet2_free(staticPacket);
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
