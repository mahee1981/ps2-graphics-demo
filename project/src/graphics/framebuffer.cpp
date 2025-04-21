#include "graphics/framebuffer.hpp"

namespace Buffers {

    Framebuffer::Framebuffer()
        : GSbuffer()
        , pixelStorageMethod(GSPixelStorageMethod::PSM_32)
    {
    }
    
    Framebuffer::Framebuffer(unsigned int width, unsigned int height, unsigned int mask, GSPixelStorageMethod gsPSM)
        : GSbuffer(width, height, mask)
        , pixelStorageMethod(gsPSM)
    {
    }
    
    Framebuffer::~Framebuffer()
    {
    }
    
    void Framebuffer::SetFramebufferAsActiveFilteredMode()
    {
        graph_set_framebuffer_filtered(this->address, this->width, static_cast<int>(this->pixelStorageMethod), 0, 0);
    }
    
    void Framebuffer::SetFramebufferAsActive(int context)
    {
        graph_set_framebuffer(context, this->address, this->width, static_cast<int>(this->pixelStorageMethod), 0, 0);
    }
    
    int Framebuffer::AllocateVRAMForBuffer()
    {
        address = graph_vram_allocate(this->width, this->height, static_cast<int>(this->pixelStorageMethod), GRAPH_ALIGN_PAGE);
        return address;
    }
    void Framebuffer::ToSDKFramebuffer(framebuffer_t* fb)
    {
    
        fb->address = this->address;
        fb->width = width;
        fb->height = this->height;
        fb->psm = static_cast<int>(this->pixelStorageMethod);
        fb->mask = this->mask;
    }
    
    u64 Framebuffer::GetFrameBufferSettings() const
    {
        return (u64(mask) << 32 | (static_cast<u64>(pixelStorageMethod) & 0x3F) << 24 | u64((width >> 6) & 0x3F) << 16 | u64((address >> 11) & 0x1FF));
    }
    
}
