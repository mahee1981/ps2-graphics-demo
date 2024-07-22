#pragma once
#include <memory>
#include <framebuffer.hpp>
#include <zbuffer.hpp>
#include <packet2.h>
#include <gif_tags.h>
#include <gs_gp.h>
#include <utils.hpp>
#include <draw.h>
#include <dma.h>
#include <AlphaTest.hpp>
#include <debug.h>


using namespace Buffers;

class DrawingEnvironment{

private:
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<ZBuffer> zbuffer;
    std::shared_ptr<AlphaTest> alphaTest;
    float xOffset;
    float yOffset;
    unsigned int context;

    u64 GetXYOffsetSettings();
    u64 GetScissoringAreaSettings();
    u64 GetAlphaAndDepthTestSettings();
    u64 GetFogColorSettings(u8 r, u8 g, u8 b);
    u64 GetDefaultAlphaBlendingSettings();
    u64 GetTextureWrappingSettings(TextureWrappingOptions wrapOptions, unsigned int minU = 0, unsigned int maxU = 0, unsigned int minV = 0, unsigned int maxV = 0);

public:
    DrawingEnvironment(std::shared_ptr<Framebuffer>, std::shared_ptr<ZBuffer> zbuffer, std::shared_ptr<AlphaTest> alphaTest);
    void SetupDrawingEnvironment(unsigned int context);
    

};