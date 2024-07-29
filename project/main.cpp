#include <framebuffer.hpp>
#include <zbuffer.hpp>
#include <DrawingEnvironment.hpp>

#include <dma.h>

#include <memory>
#include <vector>
#include <chrono>

#include <stdio.h>
#include <vector>
#include <packet2.h>
#include <packet.h>
#include <debug.h>
#include <draw.h>
#include <cmath>

constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

const float pi = atan(1) * 4.0f;

float ToRadians(float angleDegrees)
{
    return angleDegrees * pi / 180.f;
}

void InitializeDMAC()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void DumpPackets(packet2_t *packet)
{
    qword_t *qword = packet->base;
    while (qword != packet->next)
    {
        scr_printf("%lld %lld\n", qword->dw[0], qword->dw[1]);
        qword++;
    }
}

std::shared_ptr<DrawingEnvironment> InitalizeGS()
{

    std::shared_ptr<Buffers::Framebuffer> framebuffer = std::make_shared<Buffers::Framebuffer>(width, height, 0, Buffers::GSPixelStorageMethod::PSM_32);
    std::shared_ptr<Buffers::ZBuffer> zbuffer = std::make_shared<Buffers::ZBuffer>(width, height, 0, true, Buffers::ZbufferTestMethod::GREATER_EQUAL, Buffers::GSZbufferStorageMethodEnum::ZBUF_32);
    AlphaTest alphaTest = AlphaTest(true, AlphaTestMethod::NOT_EQUAL, 0x00, AlphaTestOnFail::FB_UPDATE_ONLY);

    graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, width, height); // TODO: learn more about this in docs
    graph_set_bgcolor(0, 0, 0);
    framebuffer->SetFramebufferAsActiveFilteredMode();
    graph_enable_output();

    return std::make_shared<DrawingEnvironment>(framebuffer, zbuffer, alphaTest);
}

void RenderTriangle(packet2_t *dmaBuffer, float angle)
{
    std::vector<float> triangleData{
        // x     y     z     r     g     b
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 100.f, 0.0f, 0.0f, 1.0f, 0.0f,
        100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f

    };

    qword_t qword;
    // 0xB = draw triangle and use Gouraud to get the color interpolation
    qword.dw[0] = (u64)GIF_SET_TAG(1, false, true, 0xB, GIF_FLG_PACKED, 6);
    qword.dw[1] = u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2) << 12 | u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);
    packet2_add_u128(dmaBuffer, qword.qw);

    for (std::size_t i = 0; i < triangleData.size(); i += 6)
    {
        // color
        qword.dw[0] = (u64(triangleData[i + 4] * 255.0f) & 0xFF) << 32 | (u64(triangleData[i + 3] * 255.0f) & 0xFF);
        qword.dw[1] = (u64(0x40 & 0xFF) << 32 | (u64(triangleData[i + 5] * 255.0f) & 0xFF));
        packet2_add_u128(dmaBuffer, qword.qw);

        // coordinates
        qword.dw[0] = (u64(Utils::FloatToFixedPoint<u16>((100.0f * sin(angle) + triangleData[i + 1] + yOff)))) << 32 | (u64(Utils::FloatToFixedPoint<u16>(100.0f * sin(angle) + triangleData[i] + xOff)));
        qword.dw[1] = u64(triangleData[i + 2]) & 0xFFFFFFFF;

        if (i == triangleData.size() - 6)
        {
            qword.dw[1] |= (u64(1 & 0x01) << 48); // drawing kick :)
        }
        packet2_add_u128(dmaBuffer, qword.qw);
    }
    packet2_update(dmaBuffer, draw_finish(dmaBuffer->next));
}

void SendGIFPacketWaitForDraw(packet2_t *dmaBuffer)
{

    dma_channel_send_normal(DMA_CHANNEL_GIF, dmaBuffer->base, packet2_get_qw_count(dmaBuffer), 0, 0);

    draw_wait_finish();

    graph_wait_vsync();
}

int main(int argc, char *argv[])
{

    packet2_t *myDMABuffer = packet2_create(50, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

    InitializeDMAC();

    printf("Starting to init\n");

    std::shared_ptr<DrawingEnvironment> drawEnv = InitalizeGS();

    drawEnv->SetupDrawingEnvironment(0);

    printf("Done init\n");

    drawEnv->SetClearScreenColor(0, 0, 0);

    drawEnv->ClearScreen(myDMABuffer);

    packet2_update(myDMABuffer, draw_finish(myDMABuffer->next));

    SendGIFPacketWaitForDraw(myDMABuffer);

    scr_printf("Successfully initialized the PS2 Graphics Synthesizer!\n");
    scr_printf("Also rendered a triangle!\n");

    std::chrono::duration<float> deltaTime;
    std::chrono::steady_clock::time_point lastUpdate;
    auto now = std::chrono::steady_clock::now();
    lastUpdate = now;
    float angle = 0.0f;

    while (1)
    {

        // reset the buffer
        packet2_reset(myDMABuffer, true);

        now = std::chrono::steady_clock::now();
        deltaTime = (now - lastUpdate);
        lastUpdate = now;

        angle += (1.0f * std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count()) / 1000.0f;

        if (angle > 360.0f)
        {
            angle = 0.0f;
        }

        drawEnv->ClearScreen(myDMABuffer);
        RenderTriangle(myDMABuffer, angle);

        SendGIFPacketWaitForDraw(myDMABuffer);
    }

    packet2_free(myDMABuffer);
    return 0;
}