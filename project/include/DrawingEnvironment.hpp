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
#include <TextureConfig.hpp>
#include <BlendingConfig.hpp>
#include <Color.hpp>

using namespace Buffers;

class DrawingEnvironment{

private:
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<ZBuffer> zbuffer;
    AlphaTest alphaTest;
    Colors::Color clearScreenColor;
    float xOffset;
    float yOffset;
    unsigned int context;

    u64 GetXYOffsetSettings() const;
    u64 GetScissoringAreaSettings() const;
    u64 GetAlphaAndDepthTestSettings() const;
    u64 GetFogColorSettings(u8 r, u8 g, u8 b) const; 
    u64 GetDefaultAlphaBlendingSettings() const;
    u64 GetTextureWrappingSettings(TextureWrappingOptions wrapOptions, unsigned int minU = 0, unsigned int maxU = 0, unsigned int minV = 0, unsigned int maxV = 0) const;
    u64 GetDisabledAlphaAndDepthTestSettings() const;
public:
    DrawingEnvironment(std::shared_ptr<Framebuffer>, std::shared_ptr<ZBuffer> zbuffer, AlphaTest alphaTest);
    void SetupDrawingEnvironment(unsigned int context) const;
    void ClearScreen(packet2_t *packet) const;
    void SetClearScreenColor(unsigned char r, unsigned char g, unsigned char b);

};