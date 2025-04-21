#include "graphics/DrawingEnvironment.hpp"


DrawingEnvironment::DrawingEnvironment(unsigned int width, unsigned int height, GraphicsConfig config) : 
    width(width),
    height(height),
    config(config),
    xOffset(2048.0f - float(width >> 1)),     
    yOffset(2048.0f - float(height >> 1)),
    context(0),
    framebuffer(nullptr),
    zbuffer(nullptr),
    alphaTest(true, AlphaTestMethod::NOT_EQUAL, 0x00, AlphaTestOnFail::FB_UPDATE_ONLY) //TODO: Update via config, for now hardcoded
{
    ConfigureBuffers();
}

void DrawingEnvironment::InitializeEnvironment()
{
    AllocateBuffers();
    
    ConfigureOutput();

    if(config == GraphicsConfig::SINGLE_BUFFER)
    {
        framebuffer->SetFramebufferAsActiveFilteredMode();
    }

    graph_enable_output();

    if(config == GraphicsConfig::SINGLE_BUFFER)
    {
        SetupGSRegisters(0);
    }
}

void DrawingEnvironment::ConfigureBuffers()
{
    if(config == GraphicsConfig::SINGLE_BUFFER)
    {
        framebuffer = std::make_unique<Framebuffer>(width, height, 0, Buffers::GSPixelStorageMethod::PSM_32);
        zbuffer = std::make_unique<ZBuffer>(width, height, 0, true, Buffers::ZbufferTestMethod::GREATER_EQUAL, Buffers::GSZbufferStorageMethodEnum::ZBUF_32);
    }
}

void DrawingEnvironment::AllocateBuffers()
{
  framebuffer->AllocateVRAMForBuffer();
  zbuffer->AllocateVRAMForBuffer();
}

void DrawingEnvironment::ConfigureOutput()
{
  graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
  graph_set_screen(0, 0, width, height); // TODO: learn more about this in docs
  graph_set_bgcolor(0, 0, 0);
}

void DrawingEnvironment::SetupGSRegisters(unsigned int context) const
{
    packet2_t *packet = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

    /*
    initialzies the GS based on the framebuffer and zbuffer settings
    We are sending the data in packed mode, which means that the upper part
    of the quadword contains the register to set, and the lower part contians
    the data that will be set to that address
    */
    int numOfRegistersToSet = 16;

    // GIF TAG HEADER
    qword_t qword;
    qword.dw[0] = (u64)GIF_SET_TAG(numOfRegistersToSet, true, false, 0, GIF_FLG_PACKED, 1);
    qword.dw[1] = (u64)GIF_REG_AD;
    packet2_add_u128(packet, qword.qw);

    //configure Framebuffer
    qword.dw[0] = framebuffer->GetFrameBufferSettings();
    qword.dw[1] = u64(GS_REG_FRAME + context);
    packet2_add_u128(packet, qword.qw);

    //configure Zbuffer
    qword.dw[0] = zbuffer->GetZbufferSettings();
    qword.dw[1] = u64(GS_REG_ZBUF + context);
    packet2_add_u128(packet, qword.qw);
    
    //set global primitve attributes and disable manual override
    qword.dw[0] = u64(false & 0x01);
    qword.dw[1] = u64(GS_REG_PRMODECONT);
    packet2_add_u128(packet, qword.qw);

    //Enable gouraud shading and alpha blending
    qword.dw[0] = u64(1) << 6 |  u64(0x01) << 3;
    qword.dw[1] = u64(GS_REG_PRMODE);
    packet2_add_u128(packet, qword.qw);

    qword.dw[0] = this->GetXYOffsetSettings();
    qword.dw[1] = u64(GS_REG_XYOFFSET + context);
    packet2_add_u128(packet, qword.qw);

    qword.dw[0] = this->GetScissoringAreaSettings();
    qword.dw[1] = u64(GS_REG_SCISSOR + context);
    packet2_add_u128(packet, qword.qw);

    qword.dw[0] = this->GetAlphaAndDepthTestSettings();
    qword.dw[1] = u64(GS_REG_TEST);
    packet2_add_u128(packet, qword.qw);

    //Distant fog color is set to black. I have no idea what I am doing here
    qword.dw[0] = this->GetFogColorSettings(0, 0, 0);
    qword.dw[1] = u64(GS_REG_FOGCOL);
    packet2_add_u128(packet, qword.qw);

    //disable per pixel alpha blending by default
    qword.dw[0] = u64(false);
    qword.dw[1] = u64(GS_REG_PABE);
    packet2_add_u128(packet, qword.qw);

    //set default alpha blending Color_source * Alpha_source + Color_destination(0x80 - Alpha_source)
    // TODO: add configurable alpha blending settings
    qword.dw[0] = this->GetDefaultAlphaBlendingSettings();
    qword.dw[1] = u64(GS_REG_ALPHA + context);
    packet2_add_u128(packet, qword.qw);


    //dithering settings, I know how it works, but I don't know where they got this matrix from
    //anyway it's disabled because I am using 24-bit and 32-bit colors, so it doesn't matter?
    qword.dw[0] = 0;
    qword.dw[1] = u64(GS_REG_DTHE);
    packet2_add_u128(packet, qword.qw);

    //i will not set the DIMX and die peacefully on this hill

    //color clamping (colors do not wrap around)
    qword.dw[0] = u64(true) & 0x01;
    qword.dw[1] = u64(GS_REG_COLCLAMP);
    packet2_add_u128(packet, qword.qw);


    //we don't need alpha value correction, since we are using 32-bit colors
    qword.dw[0] = u64(false) & 0x01;
    qword.dw[1] = u64(GS_REG_FBA + context);
    packet2_add_u128(packet, qword.qw);

    //texture wrapping
    qword.dw[0] = this->GetTextureWrappingSettings(TextureWrappingOptions::REPEAT);
    qword.dw[1] = u64(GS_REG_CLAMP);
    packet2_add_u128(packet, qword.qw);

    //texture alpha value settings
    qword.dw[0] = u64(0x80 & 0xFF) << 32 | (u64(false) & 0x01) << 15 | u64(0x80 & 0xFF);
    qword.dw[1] = u64(GS_REG_TEXA);
    packet2_add_u128(packet, qword.qw);

    //set the finish event to equal true
    qword.dw[0] = u64(1);
    qword.dw[1] = u64(GS_REG_FINISH);
    packet2_add_u128(packet, qword.qw);

    // Now send the packet, no need to wait since it's the first.
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->base, packet2_get_qw_count(packet), 0, 0);

    dma_wait_fast();

    packet2_free(packet);
}

void DrawingEnvironment::ClearScreen(packet2_t *packet) const
{
    qword_t qword;

    qword.dw[0] = (u64)GIF_SET_TAG(1, false, false, 0, GIF_FLG_PACKED, 1);
    qword.dw[1] = (u64)GIF_REG_AD;
    packet2_add_u128(packet, qword.qw);

    qword.dw[0] = this->GetDisabledAlphaAndDepthTestSettings();
    qword.dw[1] = u64(GS_REG_TEST);
    packet2_add_u128(packet, qword.qw);

    packet2_update(packet, draw_clear( packet->next,0,
                                       xOffset,yOffset,framebuffer->GetWidth(),framebuffer->GetHeight(),
                                       clearScreenColor.GetComponentValueAsUByte(Colors::ColorComponent::Red),
                                       clearScreenColor.GetComponentValueAsUByte(Colors::ColorComponent::Green),
                                       clearScreenColor.GetComponentValueAsUByte(Colors::ColorComponent::Blue)  ));

    
    qword.dw[0] = (u64)GIF_SET_TAG(1, false, false, 0, GIF_FLG_PACKED, 1);
    qword.dw[1] = (u64)GIF_REG_AD;
    packet2_add_u128(packet, qword.qw);

    qword.dw[0] = this->GetAlphaAndDepthTestSettings();
    qword.dw[1] = u64(GS_REG_TEST);
    packet2_add_u128(packet, qword.qw);
}

void DrawingEnvironment::SetClearScreenColor(unsigned char r, unsigned char g, unsigned char b)
{
    this->clearScreenColor.SetColor(r,g,b,0x80);
}

u64 DrawingEnvironment::GetXYOffsetSettings() const
{
    return ((u64(Utils::FloatToFixedPoint<u16>(yOffset) & 0xFFFF) << 32)  | u64(Utils::FloatToFixedPoint<u16>(xOffset) & 0xFFFF));
}

u64 DrawingEnvironment::GetScissoringAreaSettings() const
{
    return ( (u64((framebuffer->GetHeight() - 1) & 0x7FF) << 48)   
             | (u64(0 & 0x7FF) << 32) 
             | (u64((framebuffer->GetWidth() - 1) & 0x7FF) << 16) 
             | u64(0 & 0x7FF) );
}

u64 DrawingEnvironment::GetAlphaAndDepthTestSettings() const
{
    return (static_cast<u64>(zbuffer->GetDepthTestMethod()) & 0x03) << 17 
            | u64(0x01) << 16 
            | alphaTest.GetAlphaTestSettings();
}

u64 DrawingEnvironment::GetDisabledAlphaAndDepthTestSettings() const
{
    return (static_cast<u64>(ZbufferTestMethod::ALLPASS) & 0x03) << 17 
            | u64(0x01) << 16 
            | alphaTest.GetAlphaTestSettings();
}
 
u64 DrawingEnvironment::GetFogColorSettings(u8 r, u8 g, u8 b) const
{
    return  u64(b) << 16 | u64(g) << 8 | u64(r);
}
// TODO: make this customizable
u64 DrawingEnvironment::GetDefaultAlphaBlendingSettings() const
{
    return u64(0x80 & 0xFF) << 32 
           | (static_cast<u64>(AlphaBlendingColorConfig::COLOR_DESTINATION) & 0x03) << 6 
           | (static_cast<u64>(AlphaBlendingAlphaConfig::ALPHA_SOURCE) & 0x03) << 4 
           | (static_cast<u64>(AlphaBlendingColorConfig::COLOR_DESTINATION) & 0x03) << 2 
           | (static_cast<u64>(AlphaBlendingColorConfig::COLOR_SOURCE) & 0x03);
}

u64 DrawingEnvironment::GetTextureWrappingSettings(TextureWrappingOptions wrapOptions, unsigned int minU, unsigned int maxU, unsigned int minV, unsigned int maxV) const
{
   return ((static_cast<u64>(maxV) & 0x3FF) << 34)
            | ((static_cast<u64>(minV) & 0x3FF) << 24)
            | ((static_cast<u64>(maxU) & 0x3FF) << 14)
            | ((static_cast<u64>(minU) & 0x3FF) << 4)
            | ((static_cast<u64>(wrapOptions) & 0x03) << 2)
            | (static_cast<u64>(wrapOptions) & 0x03);
}
