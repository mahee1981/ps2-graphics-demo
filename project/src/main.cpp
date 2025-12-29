#define STB_IMAGE_IMPLEMENTATION

#include <dma.h>
#include <draw.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>
#include <debug.h>

#include <chrono>
#include <memory>
#include <vector>

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "graphics/DrawingEnvironment.hpp"
#include "graphics/STBITextureLoader.hpp"
#include "graphics/Texture.hpp"
#include "input/padman.hpp"
#include "light/BaseLight.hpp"
#include "light/Lighting.hpp"
#include "logging/log.hpp"
#include "renderer/Camera.hpp"
#include "renderer/model.hpp"
#include "renderer/renderer3d.hpp"
#include "tools/Deltawatch.hpp"
#include "utils.hpp"

using namespace Input;
using namespace Renderer;

constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

static u64 trianglesRendered = 0;

void InitializeDMAC()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void AddVertexToDisplayList(packet2_t *dmaBuffer,
                            const texel_t &texel,
                            const ps2math::Vec4 vertex,
                            const ps2math::Vec4 &lightColor,
                            bool kickVertex = false)
{
    // This line is a reminder of how incredibly silly decisions
    // can have you lose three months of development time on PS2
    // game engines
    static constexpr texel_t zeroTexel = {.u = 1.0f, .v = 1.0f};
    qword_t qword;
    qword.dw[0] = texel.uv;
    qword.dw[1] = zeroTexel.uv;
    packet2_add_u128(dmaBuffer, qword.qw);

    // color
    // const ps2math::Vec4 &color = model.GetColorPositions()[mesh.VertexIndices[i]];
    qword.dw[0] = (u64(lightColor.b * 128) & 0xFF) << 32 | (u64(lightColor.r * 128) & 0xFF);
    qword.dw[1] = (u64(0x80) & 0xFF) << 32 | (u64(lightColor.g * 128) & 0xFF);
    packet2_add_u128(dmaBuffer, qword.qw);

    // this copy is gonna be a performance killer, will not happen on VU1, but guarantees that it is 128-bit aligned
    // this is the one line of code that keeps my entire math from falling apart
    qword.dw[0] =
        (u64(Utils::FloatToFixedPoint<u16>((vertex.y)))) << 32 | (u64(Utils::FloatToFixedPoint<u16>(vertex.x)));
    qword.dw[1] = static_cast<unsigned int>(vertex.z);

    if (kickVertex)
    {
        qword.dw[1] |= (u64(1 & 0x01) << 48); // on every third vertex send a drawing kick :)
    }
    packet2_add_u128(dmaBuffer, qword.qw);
}
void GenerateTriangleDisplayListForModel(packet2_t *dmaBuffer,
                                         const ps2math::Mat4 &mvp,
                                         float angle,
                                         float moveHorizontal,
                                         const Mesh &mesh,
                                         const Model &model,
                                         const std::vector<ps2math::Vec4> transformedVertices,
                                         const std::vector<ps2math::Vec4> &lightColors)
{
    qword_t *header = dmaBuffer->next;
    packet2_advance_next(dmaBuffer, sizeof(u128));
    unsigned int numberOfTimesGifTagExecutes = (mesh.VertexIndices.size() + 1) / 3;

    for (std::size_t i = 0; i < mesh.VertexIndices.size(); i += 3)
    {

        const ps2math::Vec4 &v0 = transformedVertices[mesh.VertexIndices[i]];
        const ps2math::Vec4 &v1 = transformedVertices[mesh.VertexIndices[i + 1]];
        const ps2math::Vec4 &v2 = transformedVertices[mesh.VertexIndices[i + 2]];

        if (v0.w == 0.0f || v1.w == 0.0f || v2.w == 0.0f)
        {
            --numberOfTimesGifTagExecutes;
            continue;
        }

        float triArea = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
        // Y-axis in screen space is pointing down, so the culling sign is inverted
        if (triArea > 0.0f)
        {
            --numberOfTimesGifTagExecutes;
            continue;
        }

        const auto &t0 = model.GetTexturePositions()[mesh.TexIndices[i]];
        const auto &t1 = model.GetTexturePositions()[mesh.TexIndices[i + 1]];
        const auto &t2 = model.GetTexturePositions()[mesh.TexIndices[i + 2]];

        const ps2math::Vec4 &lightColor0 = lightColors[mesh._normalIndices[i]];
        const ps2math::Vec4 &lightColor1 = lightColors[mesh._normalIndices[i + 1]];
        const ps2math::Vec4 &lightColor2 = lightColors[mesh._normalIndices[i + 2]];

        AddVertexToDisplayList(dmaBuffer, t0, v0, lightColor0);
        AddVertexToDisplayList(dmaBuffer, t1, v1, lightColor1);
        AddVertexToDisplayList(dmaBuffer, t2, v2, lightColor2, true);

        trianglesRendered++;
    }
    // PRIM REG = 0x5B -> triangle with Gouraud shading, texturing, alpha blending
    header->dw[0] = (u64)GIF_SET_TAG(numberOfTimesGifTagExecutes, false, true, 0x5B, GIF_FLG_PACKED, 9);

    // constexpr u64 triangleGIFTag =  u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2)<< 12 |
    // u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);

    constexpr u64 triangleGIFTag = u64(GIF_REG_XYZ2) << 32 | u64(GIF_REG_RGBAQ) << 28 | u64(GIF_REG_ST) << 24 |
                                   u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_ST) << 12 |
                                   u64(GIF_REG_XYZ2) << 8 | u64(GIF_REG_RGBAQ) << 4 | u64(GIF_REG_ST);
    header->dw[1] = triangleGIFTag;
}

Camera SetupCamera()
{
    ps2math::Vec4 startPositon{0.0f, 0.0f, 0.0f, 1.0f};
    ps2math::Vec4 startUp{0.0f, 1.0f, 0.0f, 1.0f};
    float startYaw = -90.0f;
    float startPitch = 0.0f;
    float movementSpeed = 0.05f;
    float turnSpeed = 0.05f;

    return Camera(startPositon, startUp, startYaw, startPitch, movementSpeed, turnSpeed);
}

void render()
{
    // TODO: need to make transfers dynamic and split it into slices if bigger than u16_max
    // OPTIONAL: make packed DMA transfers
    std::array<packet2_t *, 2> drawBuffer;
    constexpr u16 MAX_PACKET_SIZE = 65000;

    drawBuffer[0] = packet2_create(MAX_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    drawBuffer[1] = packet2_create(MAX_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

    InitializeDMAC();

    LOG_INFO("Starting to init GS and draw environment");

    auto drawEnv = DrawingEnvironment(width, height, BufferingConfig::DOUBLE_BUFFER);
    auto renderer3d = Renderer3D(width, height);

    drawEnv.InitializeEnvironment();

    LOG_INFO("Done init");

    drawEnv.SetClearScreenColor(0, 0, 0);

    drawEnv.ClearScreen(drawBuffer[0]);

    packet2_update(drawBuffer[0], draw_finish(drawBuffer[0]->next));

    dma_wait_fast();
    dma_channel_send_packet2(drawBuffer[0], DMA_CHANNEL_GIF, 0);

    packet2_reset(drawBuffer[0], false);

    std::size_t numOfDMAFlushes = 0;

    float angle = 0.0f;

    float moveHorizontal = 0.0f;

    auto textureLoader = std::make_shared<graphics::STBITextureLoader>();
    graphics::Texture myTex("CAT/TEX_CAT.PNG");
    myTex.LoadTexture(textureLoader);
    myTex.AllocateVram();
    myTex.TransferTextureToGS();
    myTex.SetTexSamplingMethodInGS();
    myTex.SetTextureAsActive();

    Model myModel(ps2math::Vec4{0.0f, 0.0f, 70.0f, 1.0f});

    // TODO: fix the default path search
    // myModel.LoadModel("CAT/MESH_CAT.OBJ", "CAT/");
    // myModel.LoadModel("HITBOX/manInTheBox.obj", "HITBOX/");
    myModel.LoadModel("RIFLE/RIFLE.OBJ", "RIFLE/");
    // myModel.LoadModel("AIRPLANE/AIRPLANE.OBJ", "AIRPLANE/");
    LOG_INFO("Mesh List count: ") << myModel.GetMeshList().size();
    Mesh firstMesh = myModel.GetMeshList()[0];

    // Setup lighting
    Light::BaseLight mainLight;
    mainLight.SetColor(1.0f, 1.0f, 1.0f);
    mainLight.SetDirection(0.0f, -1.0f, -1.0f);
    mainLight.SetAmbientIntensity(0.3f);
    mainLight.SetDiffuseIntensity(0.7f);

    Deltawatch deltaWatch;

    PadManager controllerInput;
    auto myCamera = SetupCamera();
    unsigned int curr = 0;
    while (1)
    {
        trianglesRendered = 0;
        const float deltaMs = deltaWatch.GetDeltaMs();

        deltaWatch.CaptureStartMoment();

        controllerInput.UpdatePad();
        
        angle += (10.0f * deltaMs) / 100.0f;

        if (controllerInput.getPressed().DpadRight == 1)
        {
            moveHorizontal += 0.01f * deltaMs;
        }
        else if (controllerInput.getPressed().DpadLeft == 1)
        {
            moveHorizontal += -0.01f * deltaMs;
        }

        if (angle > 360.0f)
        {
            angle = 0.0f;
        }

        myCamera.MotionControl(controllerInput.getLeftJoyPad(), deltaMs);
        myCamera.RotationControl(controllerInput.getRightJoyPad(), deltaMs);

        drawEnv.ClearScreen(drawBuffer[curr]);
        dma_wait_fast();
        dma_channel_send_packet2(drawBuffer[curr], DMA_CHANNEL_GIF, 0);
        packet2_reset(drawBuffer[curr], false);
        for (int j = 0; j < 4; j++)
        {

            Components::Transform &transformComponentRef = myModel.GetTransformComponent();
            transformComponentRef.SetScaleFactor(2.5f);
            transformComponentRef.SetAngleZ(180.0f);
            transformComponentRef.SetAngleY(angle);
            transformComponentRef.SetTranslate(0.0f, j * 20.0f, 70.0f + moveHorizontal);

            // TODO: to be handled by transform system
            myModel.Update();
            // When in doubt, remember, you can get free performance by
            // not having this in the vertex loop
            ps2math::Mat4 mvp =
                myModel.GetWorldMatrix() * myCamera.CalculateViewMatrix() * renderer3d.GetPerspectiveMatrix();

            auto transformedVertices = myModel.TransformVertices(mvp, width, height, xOff, yOff);

            // Pre-calculate lighting for all normals
            std::vector<ps2math::Vec4> lightColors(myModel.GetVertexNormals().size(), ps2math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            for (size_t i = 0; i < myModel.GetVertexNormals().size(); ++i)
            {
                ps2math::Vec4 transformedNormal = myModel.GetVertexNormals()[i] * myModel.GetWorldMatrix();
                lightColors[i] = Light::CalculateLighting(transformedNormal.Normalize(), mainLight);
            }

            for (const auto &mesh : myModel.GetMeshList())
            {
                u32 qwords_needed = 1 + (mesh.VertexIndices.size() * 3); // 1 GIF Tag + 3 qwords per vertex
                u32 qwords_used = packet2_get_qw_count(drawBuffer[curr]);
                if (qwords_used + qwords_needed + 10 > MAX_PACKET_SIZE)
                {
                    dma_wait_fast();
                    dma_channel_send_packet2(drawBuffer[curr], DMA_CHANNEL_GIF, 0);
                    packet2_reset(drawBuffer[1 - curr], false);
                    curr = 1 - curr;
                }
                GenerateTriangleDisplayListForModel(drawBuffer[curr],
                                                    mvp,
                                                    angle,
                                                    moveHorizontal,
                                                    mesh,
                                                    myModel,
                                                    transformedVertices,
                                                    lightColors);
            }
        }
        packet2_update(drawBuffer[curr], draw_finish(drawBuffer[curr]->next));

        dma_wait_fast();
        dma_channel_send_packet2(drawBuffer[curr], DMA_CHANNEL_GIF, 0);
        packet2_reset(drawBuffer[curr], false);

        deltaWatch.CaptureEndMoment();
        draw_wait_finish();
        graph_wait_vsync();
        scr_setXY(0,0);
        scr_printf("Time to render the frame: %f\n", deltaMs);
        scr_printf("Triangles sent to GS: %llu", trianglesRendered);
        drawEnv.SwapBuffers();
    }
    packet2_free(drawBuffer[0]);
    packet2_free(drawBuffer[1]);
}

int main(int argc, char *argv[])
{
    render();

    return 0;
}
