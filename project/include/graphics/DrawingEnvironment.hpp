#ifndef DRAWING_ENVIRONMENT_HPP
#define DRAWING_ENVIRONMENT_HPP

#include <debug.h>
#include <dma.h>
#include <draw.h>
#include <gif_tags.h>
#include <gs_gp.h>
#include <packet2.h>
#include <utils.hpp>

#include <array>
#include <memory>

#include "AlphaTest.hpp"
#include "Color.hpp"
#include "TextureConfig.hpp"
#include "graphics/GraphicsConfig.hpp"
#include "graphics/framebuffer.hpp"
#include "graphics/zbuffer.hpp"

using namespace Buffers;

class DrawingEnvironment
{

  private:
    unsigned int width, height;
    GraphicsConfig config;
    float xOffset;
    float yOffset;
    unsigned int context;
    std::array<std::unique_ptr<Framebuffer>, 2> framebuffer;
    std::unique_ptr<ZBuffer> zbuffer;
    AlphaTest alphaTest;
    Colors::Color clearScreenColor;
    packet2_t *flipPacket;

    u64 GetXYOffsetSettings() const;
    u64 GetScissoringAreaSettings() const;
    u64 GetAlphaAndDepthTestSettings() const;
    u64 GetFogColorSettings(u8 r, u8 g, u8 b) const;
    u64 GetDefaultAlphaBlendingSettings() const;
    u64 GetTextureWrappingSettings(TextureWrappingOptions wrapOptions,
                                   unsigned int minU = 0,
                                   unsigned int maxU = 0,
                                   unsigned int minV = 0,
                                   unsigned int maxV = 0) const;
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
    void SwapBuffers();
    DrawingEnvironment &operator=(const DrawingEnvironment &other) = delete;
    DrawingEnvironment(const DrawingEnvironment &other) = delete;
};

#endif
