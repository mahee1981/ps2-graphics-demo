#include <cstddef>
#include <cstdio>
#include <packet2_utils.h>
#include <debug.h>

#include <memory>

#include "renderer/vu1renderer3dbase.hpp"
#include "VU0Math/vec4.hpp"
#include "graphics/Texture.hpp"
#include "graphics/TextureCache.hpp"
#include "graphics/VramManager.hpp"
#include "logging/log.hpp"
#include "utils.hpp"

namespace Renderer
{
VU1Renderer3DBase::VU1Renderer3DBase(int width, int height)
    : _screenWidth(width), _screenHeight(height),
      _perspectiveMatrix(
          ps2math::Mat4::perspective(Utils::ToRadians(60.0f), (float)width / (float)height, 1.0f, 2000.0f)),
      dynamicPacket{SmartPacket{packet2_create(250, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free},
                    SmartPacket{packet2_create(250, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free}},
      staticScaleAndColorPacket(SmartPacket(packet2_create(20, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1), &packet2_free)),
      context(0), isDebuggingEnabled(true), textureCache(nullptr)
{

    _perspectiveMatrix = ps2math::Mat4::SpecializePerspectiveForVU1(_perspectiveMatrix, width, height);
    PrepareScaleAndColorPacket();
}
void VU1Renderer3DBase::SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far)
{
    _perspectiveMatrix = ps2math::Mat4::perspective(angleRadians, aspectRatio, near, far);
}
void VU1Renderer3DBase::UploadVU1MicroProgram(u32 *VU1Draw3D_CodeStart, u32 *VU1Draw3D_CodeEnd)
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

void VU1Renderer3DBase::PrepareScaleAndColorPacket()
{
    packet2_add_float(staticScaleAndColorPacket.get(), 2048.0F); // scale
    packet2_add_float(staticScaleAndColorPacket.get(), 2048.0F); // scale
    float zScale = float(0xFFFFFFFFu) / 32.0F;
    LOG_INFO("z-scale: ") << zScale;
    packet2_add_float(staticScaleAndColorPacket.get(), zScale); // scale
    packet2_add_float(staticScaleAndColorPacket.get(), 1.0f);   // scale
    u8 j = 0;                                                   // RGBA
    for (j = 0; j < 4; j++)
        packet2_add_float(staticScaleAndColorPacket.get(), 128.0f);
}

bool VU1Renderer3DBase::SendDynamicModelData(const ps2math::Mat4 &mvp,
                                             const ps2math::Mat4 &modelMatrix,
                                             const Light::BaseLight &light,
                                             const std::shared_ptr<Texture> &texture,
                                             const ps2math::Vec4 &cameraPos)
{
    // Bind before anything else: eviction can move a texture betwen frames, so
    // the Tex0 value written further down ahs to be built form the address the
    // cache just confirmed. skip the model entirely if it will not fit, rather
    // than pointing TEX0 at VRAM that now belongs to something else
    if (textureCache != nullptr && textureCache->Bind(*texture) == graphics::VramManager::INVALID)
        return false;

    packet2_t *currentVifPacket = dynamicPacket[context].get();
    packet2_reset(currentVifPacket, 0);

    packet2_chain_open_cnt(currentVifPacket, 0, 0, 0);
    packet2_vif_flush(currentVifPacket, 0);
    packet2_vif_nop(currentVifPacket, 0);
    packet2_chain_close_tag(currentVifPacket);
    // Add Model View Projection Matrix
    u32 vifAddedBytes = 0;
    packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, (void *)mvp.GetDataPtr(), 4, 0);
    vifAddedBytes += 4;

    // Merge with scale and color packet
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes, // because of the world Matrix
                                     staticScaleAndColorPacket->base,
                                     packet2_get_qw_count(staticScaleAndColorPacket.get()),
                                     0);
    vifAddedBytes += packet2_get_qw_count(staticScaleAndColorPacket.get());

    // Add Normal Matrix
    packet2_utils_vu_add_unpack_data(currentVifPacket, vifAddedBytes, (void *)modelMatrix.GetDataPtr(), 4, 0);
    vifAddedBytes += 4;

    // add Light packet
    packet2_utils_vu_add_unpack_data(currentVifPacket,
                                     vifAddedBytes, // because of the world Matrix
                                     light.GetPacketInformation()->base,
                                     packet2_get_qw_count(light.GetPacketInformation()),
                                     0);
    vifAddedBytes += packet2_get_qw_count(light.GetPacketInformation());

    // Add Texture Id GIF Tag and Camera position
    packet2_utils_vu_open_unpack(currentVifPacket, vifAddedBytes, 0);
    {
        packet2_utils_gif_add_set(currentVifPacket, 1);
        qword_t textureConfig;
        textureConfig.dw[0] = texture->GetTexGSSettings();
        textureConfig.dw[1] = GS_REG_TEX0_1;
        packet2_add_u128(currentVifPacket, textureConfig.qw);

        packet2_add_float(currentVifPacket, cameraPos.x);
        packet2_add_float(currentVifPacket, cameraPos.y);
        packet2_add_float(currentVifPacket, cameraPos.z);
        packet2_add_float(currentVifPacket, 0.0f);
    }
    packet2_utils_vu_close_unpack(currentVifPacket);

    packet2_utils_vu_add_end_tag(currentVifPacket);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);
    context = !context;

    return true;
}

void VU1Renderer3DBase::RenderFrame(const std::vector<Model> &models,
                                    const Light::BaseLight &mainLight,
                                    const ps2math::Mat4 &viewProjMatrix,
                                    const ps2math::Vec4 &cameraPos)
{
    lastDisplayListPrepWatch.CaptureStartMoment();
    const float timeToPrepLastDisplayList = lastDisplayListPrepWatch.GetDeltaMs();
    DoRenderFrame(models, mainLight, viewProjMatrix, cameraPos);
    lastDisplayListPrepWatch.CaptureEndMoment();
    graph_wait_vsync();
    if (isDebuggingEnabled)
    {
        scr_setXY(0, 0);
        std::sprintf(processTimeMessageBuff, "Time to process display list: %f [ms]", timeToPrepLastDisplayList);
        AppendScreenPrintLine(processTimeMessageBuff);

        if (textureCache != nullptr)
        {
            const graphics::TextureCache::Stats &stats = textureCache->GetStats();
            std::sprintf(textureCacheMessageBuff,
                    "Tex: %u resident, %u bind, %u uploads, %u evicted, %u KB free",
                    textureCache->GetResidentCount(),
                    stats.binds,
                    stats.uploads,
                    stats.evictions,
                    textureCache->GetVramManager().TotalFree() * 4 / 1024);
            AppendScreenPrintLine(textureCacheMessageBuff);
        }
        PrintDebugInfo();
        logStream.str(std::string());
        logStream.clear();
    }

    if (textureCache != nullptr)
        textureCache->NextFrame();
}

void VU1Renderer3DBase::SetTextureCache(graphics::TextureCache *cache)
{
    textureCache = cache;
}
void VU1Renderer3DBase::SetDoubleBufferSettings()
{
    constexpr u8 NUM_OF_MODEL_CONSTANTS = 15;
    packet2_t *packet2 = packet2_create(1, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    packet2_utils_vu_add_double_buffer(packet2, NUM_OF_MODEL_CONSTANTS, (1024 - NUM_OF_MODEL_CONSTANTS) / 2);
    packet2_utils_vu_add_end_tag(packet2);
    dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    packet2_free(packet2);
}

void VU1Renderer3DBase::ToggleDebugPrint()
{
    isDebuggingEnabled = !isDebuggingEnabled;
}

void VU1Renderer3DBase::PrintDebugInfo()
{
    scr_printf("%s", logStream.str().c_str());
}

} // namespace Renderer
