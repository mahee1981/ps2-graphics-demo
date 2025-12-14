#define STB_IMAGE_IMPLEMENTATION

#include <dma.h>
#include <draw.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>

#include <chrono>
#include <memory>
#include <vector>

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "graphics/DrawingEnvironment.hpp"
#include "graphics/STBITextureLoader.hpp"
#include "graphics/Texture.hpp"
#include "input/padman.hpp"
#include "logging/log.hpp"
#include "renderer/Camera.hpp"
#include "renderer/model.hpp"
#include "renderer/renderer3d.hpp"
#include "utils.hpp"

using namespace Input;
using namespace Renderer;

constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

void InitializeDMAC()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void ClipVertices(ps2math::Vec4 &vertex)
{
    asm volatile("lqc2       $vf7, 0x00(%0)          \n"
                 "vclipw.xyz	$vf7, $vf7           \n" // FIXME: Clip detection is still kinda broken.
                 "vnop                               \n"
                 "vnop                               \n"
                 "vnop                               \n"
                 "vnop                               \n"
                 "cfc2       $10, $18                \n"
                 "beq        $10, $0, 3f             \n"
                 "2:                                 \n"
                 "sqc2       $0, 0x00(%0)            \n"
                 "j          4f                      \n"
                 "3:                                 \n"
                 "vdiv       $Q, $vf0w, $vf7w        \n"
                 "vwaitq                             \n"
                 "vmulq.xyz  $vf7, $vf7, $Q          \n"
                 "sqc2       $vf7, 0x00(%0)          \n"
                 "4:                                 \n"
                 :
                 : "r"(&vertex)
                 : "$10", "memory");
}

void GenerateTriangleDisplayListForModel(packet2_t *dmaBuffer,
                                        const ps2math::Mat4 &mvp,
                                        float angle,
                                        float moveHorizontal,
                                        const Mesh &mesh,
                                        const Model &model)
{
    qword_t qword;
    const unsigned int numberOfTimesGifTagExecutes = (mesh.VertexIndices.size() + 1) / 3;
    // PRIM REG = 0x5B -> triangle with Gouraud shading, texturing, alpha blending
    qword.dw[0] = (u64)GIF_SET_TAG(numberOfTimesGifTagExecutes, true, true, 0x5B, GIF_FLG_PACKED, 9);

    // constexpr u64 triangleGIFTag =  u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2)<< 12 |
    // u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);

    constexpr u64 triangleGIFTag = u64(GIF_REG_XYZ2) << 32 | u64(GIF_REG_RGBAQ) << 28 | u64(GIF_REG_ST) << 24 |
                                   u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_ST) << 12 |
                                   u64(GIF_REG_XYZ2) << 8 | u64(GIF_REG_RGBAQ) << 4 | u64(GIF_REG_ST);
    qword.dw[1] = triangleGIFTag;
    packet2_add_u128(dmaBuffer, qword.qw);

    // This line is a reminder of how incredibly silly decisions
    // can have you lose three months of development time on PS2
    // game engines
    constexpr texel_t zeroTexel = {.u = 1.0f, .v = 1.0f};

    for (std::size_t i = 0; i < mesh.VertexIndices.size(); i++)
    {

        const auto &texel = model.GetTexturePositions()[mesh.TexIndices[i]];
        qword.dw[0] = texel.uv;
        qword.dw[1] = zeroTexel.uv;
        packet2_add_u128(dmaBuffer, qword.qw);

        // color
        // const ps2math::Vec4 &color = model.GetColorPositions()[mesh.VertexIndices[i]];
        // qword.dw[0] = (u64(color.b * 128) & 0xFF) << 32 | (u64(color.r * 128) & 0xFF);
        // qword.dw[1] = (u64(0x80) & 0xFF) << 32 | (u64(color.g * 128) & 0xFF);
        qword.dw[0] = (u64(128) & 0xFF) << 32 | (u64(128) & 0xFF);
        qword.dw[1] = (u64(0x80) & 0xFF) << 32 | (u64(128) & 0xFF);
        packet2_add_u128(dmaBuffer, qword.qw);

        // this copy is gonna be a performance killer, will not happen on VU1, but guarantees that it is 128-bit aligned
        // this is the one line of code that keeps my entire math from falling apart
        ps2math::Vec4 vertex = model.GetVertexPositions()[mesh.VertexIndices[i]];

        // coordinates
        vertex = vertex * mvp;
        // clip vertices and perform perspective division
        ClipVertices(vertex);

        // viewport transformation
        float winX = float(width) * vertex.x / 2.0f + (xOff);
        float winY = float(height) * vertex.y / 2.0f + (yOff);
        float deviceZ = (vertex.z + 1.0f) / 2.0f * (1 << 31);

        qword.dw[0] = (u64(Utils::FloatToFixedPoint<u16>((winY)))) << 32 | (u64(Utils::FloatToFixedPoint<u16>(winX)));
        qword.dw[1] = static_cast<unsigned int>(deviceZ);

        if ((i + 1) % 3 == 0)
        {
            qword.dw[1] |= (u64(1 & 0x01) << 48); // on every third vertex send a drawing kick :)
        }
        packet2_add_u128(dmaBuffer, qword.qw);
    }
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

    drawBuffer[0] = packet2_create(35000, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    drawBuffer[1] = packet2_create(35000, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

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

    std::chrono::duration<float, std::milli> deltaTime;
    std::chrono::steady_clock::time_point lastUpdate;
    auto now = std::chrono::steady_clock::now();
    lastUpdate = now;

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

    PadManager controllerInput;
    auto myCamera = SetupCamera();
    unsigned int curr = 0;
    while (1)
    {
        controllerInput.UpdatePad();

        now = std::chrono::steady_clock::now();
        deltaTime = (now - lastUpdate);
        lastUpdate = now;

        angle += (10.0f * deltaTime.count()) / 100.0f;

        if (controllerInput.getPressed().DpadRight == 1)
        {
            moveHorizontal += 0.01f * deltaTime.count();
        }
        else if (controllerInput.getPressed().DpadLeft == 1)
        {
            moveHorizontal += -0.01f * deltaTime.count();
        }

        if (angle > 360.0f)
        {
            angle = 0.0f;
        }

        myCamera.MotionControl(controllerInput.getLeftJoyPad(), deltaTime.count());
        myCamera.RotationControl(controllerInput.getRightJoyPad(), deltaTime.count());

        drawEnv.ClearScreen(drawBuffer[curr]);
        dma_wait_fast();
        dma_channel_send_packet2(drawBuffer[curr], DMA_CHANNEL_GIF, 0);
        packet2_reset(drawBuffer[curr], false);

        Components::Transform &transformComponentRef = myModel.GetTransformComponent();
        transformComponentRef.SetScaleFactor(2.5f);
        transformComponentRef.SetAngleZ(180.0f);
        transformComponentRef.SetAngleY(angle);
        transformComponentRef.SetTranslate(0.0f, 0.0f, 70.0f + moveHorizontal);

        // TODO: to be handled by transform system
        myModel.Update();
        // When in doubt, remember, you can get free performance by
        // not having this in the vertex loop
        ps2math::Mat4 mvp =
            myModel.GetWorldMatrix() * myCamera.CalculateViewMatrix() * renderer3d.GetPerspectiveMatrix();

        auto loopSize = myModel.GetMeshList().size();
        for (std::size_t i = 0; i < loopSize; i++)
        {
            const Mesh &mesh = myModel.GetMeshList()[i];
            GenerateTriangleDisplayListForModel(drawBuffer[curr], mvp, angle, moveHorizontal, mesh, myModel);

            if (i == loopSize - 1)
            {
                packet2_update(drawBuffer[curr], draw_finish(drawBuffer[curr]->next));
            }

            dma_wait_fast();
            dma_channel_send_packet2(drawBuffer[curr], DMA_CHANNEL_GIF, 0);
            packet2_reset(drawBuffer[1 - curr], false);
            curr = 1 - curr;
        }

        draw_wait_finish();
        graph_wait_vsync();
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
