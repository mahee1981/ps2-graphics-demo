#include "renderer/path1renderer3d.hpp"
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
    dynamicPacket[0] = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    dynamicPacket[1] = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    staticPacket = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    _perspectiveMatrix = ps2math::Mat4::SpecializePerspectiveForVU1(_perspectiveMatrix, width, height);
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
void Path1Renderer3D::RenderFrame(const std::vector<Model> &models,
                                  const Light::BaseLight &mainLight,
                                  const ps2math::Mat4 &viewMat)
{
    const auto viewProjMat = viewMat * _perspectiveMatrix;
    // constexpr u32 MAX_QWORDS_PER_VIF_PACKET = 120;
    for (const Model &model : models)
    {
        packet2_add_float(staticPacket, 2048.0F);        // scale
        packet2_add_float(staticPacket, 2048.0F);        // scale
        packet2_add_float(staticPacket, float(1 << 31) / 2.0F); // scale
        std::size_t vertexCount = model.GetMeshList()[0].Vertices.size();            
        packet2_add_s32(staticPacket, vertexCount);
        packet2_utils_gs_add_prim_giftag(staticPacket, &primitiveTypeConfig, vertexCount, DRAW_STQ2_REGLIST, 3, 0);
        u8 j = 0; // RGBA
        for (j = 0; j < 4; j++)
            packet2_add_u32(staticPacket, 128);

        ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;
        

        for (const auto &mesh : model.GetMeshList())
        {

            auto &currentVifPacket = dynamicPacket[context];
            packet2_reset(currentVifPacket, 0);

            // Add the matrix at the top of the memory and skip the TOP register shenaningans
            // reserving 10 qwords for this is an overkill, but whatever
            packet2_utils_vu_add_unpack_data(currentVifPacket, 0, mvp.GetDataPtr(), 4, 0);

            u32 vifAddedBytes = 0; // zero because now we will use TOP register (double buffer)
                                     // we don't wan't to unpack at 8 + beggining of buffer, but at
                                     // the beggining of the buffer

            // Merge packets
            packet2_utils_vu_add_unpack_data(currentVifPacket,
                                             vifAddedBytes,
                                             staticPacket->base,
                                             packet2_get_qw_count(staticPacket),
                                             1);
            vifAddedBytes += packet2_get_qw_count(staticPacket);

            // Add vertices
            packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, (void*)(mesh.Vertices.data()), vertexCount, 1);
            vifAddedBytes += vertexCount; // one VECTOR is size of qword

            // Add sts
            packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, (void*)(mesh.Texels.data()), vertexCount, 1);
            vifAddedBytes += vertexCount;

            packet2_utils_vu_add_start_program(currentVifPacket, 0);
            packet2_utils_vu_add_end_tag(currentVifPacket);
            dma_channel_wait(DMA_CHANNEL_VIF1, 0);
            dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);

            // Switch packet, so we can proceed during DMA transfer
            context = !context;
        }

        packet2_reset(staticPacket, 0);
    }
    graph_wait_vsync();


}
void Path1Renderer3D::SetDoubleBufferSettings()
{
	packet2_t *packet2 = packet2_create(1, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
	packet2_utils_vu_add_double_buffer(packet2, 8, 496);
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
    for(auto &packet : dynamicPacket)
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
