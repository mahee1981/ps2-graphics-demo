#include <debug.h>
#include <dma.h>
#include <draw.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>

#include <memory>
#include <vector>

#include "VU0Math/vec4.hpp"
#include "components/transform.hpp"
#include "graphics/DrawingEnvironment.hpp"
#include "graphics/LodeTextureLoader.hpp"
#include "graphics/STBITextureLoader.hpp"
#include "graphics/Texture.hpp"
#include "input/padman.hpp"
#include "light/BaseLight.hpp"
#include "logging/log.hpp"
#include "renderer/Camera.hpp"
#include "renderer/trianglestriprenderer3d.hpp"
#include "renderer/model.hpp"
#include "renderer/path1renderer3d.hpp"
#include "tools/Deltawatch.hpp"

using namespace Input;
using namespace Renderer;

constexpr int width = 640;
constexpr int height = 448;

extern u32 VU1Draw3DTriStrip_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3DTriStrip_CodeEnd __attribute__((section(".vudata")));

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
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    LOG_INFO("Starting to init GS and draw environment");

    DrawingEnvironment drawEnv(width, height, BufferingConfig::DOUBLE_BUFFER);
    std::unique_ptr<IRenderer3D> renderer3d = std::make_unique<Path1Renderer3D>(width, height);

    if (auto path1Renderer = dynamic_cast<Path1Renderer3D *>(renderer3d.get()); path1Renderer != nullptr)
    {
        LOG_INFO("Detected path 1 renderer");
        dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
        dma_channel_fast_waits(DMA_CHANNEL_VIF1);
        path1Renderer->UploadVU1MicroProgram(&VU1Draw3DTriStrip_CodeStart, &VU1Draw3DTriStrip_CodeEnd);
        path1Renderer->SetDoubleBufferSettings();
    }

    drawEnv.InitializeEnvironment();

    LOG_INFO("Done init");

    drawEnv.SetClearScreenColor(0, 0, 0);

    drawEnv.ClearScreen();

    auto textureLoader = std::make_shared<graphics::STBITextureLoader>();
    std::shared_ptr<graphics::Texture> catTex = std::make_shared<graphics::Texture>("CAT/TEX_CAT.PNG");
    // std::shared_ptr<graphics::Texture> airplaneTex = std::make_shared<graphics::Texture>("AIRPLANE/AIRPLANE_TEX2.PNG");
    std::shared_ptr<graphics::Texture> bigCatTex = std::make_shared<graphics::Texture>("HIDEFCAT/Cat_diffuse.jpg");
    // std::shared_ptr<graphics::Texture> brickTex = std::make_shared<graphics::Texture>("brick_wall_128.png");

    catTex->LoadTexture(textureLoader);
    catTex->AllocateVram();
    catTex->TransferTextureToGS();
    catTex->SetTexSamplingMethodInGS();

    bigCatTex->LoadTexture(textureLoader);
    bigCatTex->AllocateVram();
    bigCatTex->TransferTextureToGS();
    bigCatTex->SetTexSamplingMethodInGS();
    // Goal: Use path3 only for upload

    std::vector<Model> modelList;
    modelList.emplace_back(Model{ps2math::Vec4{0.0f, 0.0f, -70.0f, 1.0f}});
    modelList.emplace_back(Model{ps2math::Vec4(-60.0f, -90.0f, +50.0f, 1.0f)});

    // modelList[0].LoadModel("CAT/MESH_CAT.OBJ");
    modelList[0].LoadModel("HIDEFCAT/CAT.obj");
    modelList[1] = modelList[0];
    // modelList[0].LoadModel("FLOOR/FLOOR.OBJ");
    // modelList[0].LoadModel("GTR/GTR.obj");
    modelList[0].GetTransformComponent().RotateXBy(90);
    modelList[1].GetTransformComponent().RotateXBy(90);
    modelList[0].AddTexture(bigCatTex);
    modelList[1].AddTexture(bigCatTex);
    modelList[1].GetTransformComponent().SetTranslate(-60.0f, -90.0f, 50.0f);
    // modelList[0].LoadModelPackets("const char *fileName");

    // modelList[0].LoadModel("FLOOR/FLOOR.OBJ");
    // modelList[1].AddTexture(catTex);
    PadManager controllerInput{false};

    float angle = 0.0f;
    float moveHorizontal = 0.0f;

    // Setup lighting
    Light::BaseLight mainLight;
    mainLight.SetColor(1.0f, 1.0f, 1.0f);
    mainLight.SetDirection(1.0f, 0.0f, 0.0f);
    mainLight.SetAmbientIntensity(0.3f);
    mainLight.SetDiffuseIntensity(0.2f);
    mainLight.SetSpecularIntensity(0.5f);

    Deltawatch deltaWatch;

    auto myCamera = SetupCamera();
    while (1)
    {
        const float deltaMs = deltaWatch.GetDeltaMs();

        deltaWatch.CaptureStartMoment();

        controllerInput.UpdatePad();

        // angle += (10.0f * deltaMs) / 100.0f;

        // TODO: move to an input handler class
        if (controllerInput.getPressed().DpadRight == 1)
        {
            moveHorizontal = 0.01f * deltaMs;
        }
        else if (controllerInput.getPressed().DpadLeft == 1)
        {
            moveHorizontal = -0.01f * deltaMs;
        }
        else
        {
            moveHorizontal = 0.0f;
        }

        if (controllerInput.getClicked().Cross == 1)
        {
            renderer3d->ToggleDebugPrint();
        }

        if (angle > 360.0f)
        {
            angle = 0.0f;
        }

        myCamera.MotionControl(controllerInput.getLeftJoyPad(), deltaMs);
        myCamera.RotationControl(controllerInput.getRightJoyPad(), deltaMs);
        drawEnv.ClearScreen();
        for (auto &model : modelList)
        {
            Components::Transform &transformComponentRef = model.GetTransformComponent();
            // transformComponentRef.SetScaleFactor(0.5f);
            transformComponentRef.SetAngleY(15.0f);

            // transformComponentRef.SetAngleY(angle);
            transformComponentRef.SetTranslate(transformComponentRef.GetTranslate().x,
                                               transformComponentRef.GetTranslate().y,
                                               transformComponentRef.GetTranslate().z + moveHorizontal);

            // TODO: to be handled by transform system
            model.Update();
        }
        renderer3d->RenderFrame(modelList, mainLight, myCamera.CalculateViewMatrix(), myCamera.GetPosition());

        deltaWatch.CaptureEndMoment();
        drawEnv.SwapBuffers();
    }
}

int main(int argc, char *argv[])
{
    scr_setfontcolor(0xffffff);
    render();

    return 0;
}
