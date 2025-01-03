#include <DrawingEnvironment.hpp>
#include <framebuffer.hpp>
#include <zbuffer.hpp>

#include <dma.h>

#include <chrono>
#include <memory>
#include <vector>

#include <cmath>
#include <debug.h>
#include <draw.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>
#include <vector>

#include <VU0Math/mat4.hpp>
#include <VU0Math/vec4.hpp>

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

void DumpPackets(packet2_t* packet)
{
    qword_t* qword = packet->base;
    while (qword != packet->next) {
        scr_printf("%lld %lld\n", qword->dw[0], qword->dw[1]);
        qword++;
    }
}
//TODO: refactor this and maybe move away from smart pointers
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


void PrepareTriangleDisplayList(packet2_t* dmaBuffer, float angle)
{
    //constexpr int vertexDataOffset = 0;
    //constexpr int colorDataOffset = 4 * sizeof(float);

    // Data is to be stored in an obj file that has coordinates, color and texutures as Vec4, so that we get a qword alignment"
    std::vector<float> triangleData {
        // x     y     z    w     r     g     b        a
        0.0f, 100.f, 0.0f, 1.0, 0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0, 1.0f, 0.0f, 0.0f, 1.0f,
        100.0f, 0.0f, 0.0f, 1.0, 0.0f, 0.0f, 1.0f, 1.0f

    };

    qword_t qword;
    // 0xB = draw triangle and use Gouraud to get the color interpolation
    qword.dw[0] = (u64)GIF_SET_TAG(1, false, true, 0xB, GIF_FLG_PACKED, 6);
    qword.dw[1] = u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2) << 12 | u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);
    packet2_add_u128(dmaBuffer, qword.qw);

    float* begin = triangleData.data();

    for (std::size_t i = 0; i < triangleData.size(); i += 8) {
        // color
        qword.dw[0] = (u64(triangleData[i + 5] * 255.0f) & 0xFF) << 32 | (u64(triangleData[i + 4] * 255.0f) & 0xFF);
        qword.dw[1] = (u64(triangleData[i + 7] * 0x80) & 0xFF) << 32 | (u64(triangleData[i + 6] * 255.0f) & 0xFF);
        packet2_add_u128(dmaBuffer, qword.qw);

        ps2math::Vec4 vertex(begin + i);
        ps2math::Mat4 modelMatrix;
        // modelMatrix = ps2math::Mat4::rotateX(modelMatrix, ToRadians(angle));
        modelMatrix = ps2math::Mat4::rotateX(modelMatrix, ToRadians(angle));
        modelMatrix = ps2math::Mat4::translate(modelMatrix, ps2math::Vec4(angle, angle, angle, 1));
        // coordinates
        vertex = vertex * modelMatrix;
        qword.dw[0] = (u64(Utils::FloatToFixedPoint<u16>((vertex.y + yOff)))) << 32 | (u64(Utils::FloatToFixedPoint<u16>(vertex.x + xOff)));
        qword.dw[1] = u64(vertex.z) & 0xFFFFFFFF;

        if (i == triangleData.size() - 8) {
            qword.dw[1] |= (u64(1 & 0x01) << 48); // drawing kick :)
        }
        packet2_add_u128(dmaBuffer, qword.qw);
    }
    packet2_update(dmaBuffer, draw_finish(dmaBuffer->next));
}

void SendGIFPacketWaitForDraw(packet2_t* dmaBuffer)
{
    dma_wait_fast();

    dma_channel_send_normal(DMA_CHANNEL_GIF, dmaBuffer->base, packet2_get_qw_count(dmaBuffer), 0, 0);

    draw_wait_finish();

    graph_wait_vsync();
}

int main(int argc, char* argv[])
{

    packet2_t* myDMABuffer = packet2_create(50, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

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

    std::chrono::duration<float, std::milli> deltaTime;
    std::chrono::steady_clock::time_point lastUpdate;
    auto now = std::chrono::steady_clock::now();
    lastUpdate = now;
    float angle = 0.0f;

    while (1) {

        // reset the buffer
        packet2_reset(myDMABuffer, true);

        now = std::chrono::steady_clock::now();
        deltaTime = (now - lastUpdate);
        lastUpdate = now;

        angle += (10.0f * deltaTime.count()) / 1000.0f;

        if (angle > 360.0f) {
            angle = 0.0f;
        }

        drawEnv->ClearScreen(myDMABuffer);
        PrepareTriangleDisplayList(myDMABuffer, angle);

        SendGIFPacketWaitForDraw(myDMABuffer);
    }

    packet2_free(myDMABuffer);
    return 0;
}
