#ifndef DRAWING_ENVIRONMENT_HPP
#define DRAWING_ENVIRONMENT_HPP

#include <packet2.h>
#include <gif_tags.h>
#include <gs_gp.h>
#include <utils.hpp>
#include <draw.h>
#include <dma.h>
#include <debug.h>

#include <memory>

#include "graphics/zbuffer.hpp"
#include "graphics/framebuffer.hpp"
#include "AlphaTest.hpp"
#include "TextureConfig.hpp"
#include "Color.hpp"
#include "graphics/GraphicsConfig.hpp"


using namespace Buffers;

class DrawingEnvironment{

private:
    unsigned int width, height;
    GraphicsConfig config;
    float xOffset;
    float yOffset;
    unsigned int context;
    std::unique_ptr<Framebuffer> framebuffer;
    std::unique_ptr<ZBuffer> zbuffer;
    AlphaTest alphaTest;
    Colors::Color clearScreenColor;

    u64 GetXYOffsetSettings() const;
    u64 GetScissoringAreaSettings() const;
    u64 GetAlphaAndDepthTestSettings() const;
    u64 GetFogColorSettings(u8 r, u8 g, u8 b) const; 
    u64 GetDefaultAlphaBlendingSettings() const;
    u64 GetTextureWrappingSettings(TextureWrappingOptions wrapOptions, unsigned int minU = 0, unsigned int maxU = 0, unsigned int minV = 0, unsigned int maxV = 0) const;
    u64 GetDisabledAlphaAndDepthTestSettings() const;

    void ConfigureOutput();
    void ConfigureBuffers();
    void AllocateBuffers();
    void SetupGSRegisters(unsigned int context) const;

public:
    DrawingEnvironment(unsigned int width, unsigned int height, GraphicsConfig config);
    void ClearScreen(packet2_t *packet) const;
    void SetClearScreenColor(unsigned char r, unsigned char g, unsigned char b);
    void InitializeEnvironment();
    DrawingEnvironment& operator=(const DrawingEnvironment &other) = delete;
    DrawingEnvironment(const DrawingEnvironment &other) = delete;
    
};

#endif
