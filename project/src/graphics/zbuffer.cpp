#include <graphics/zbuffer.hpp>

namespace Buffers {

    ZBuffer::ZBuffer()
        : GSbuffer()
        , enable(true)
        , method(ZbufferTestMethod::GREATER_EQUAL)
        , zbufferStorageMethod(GSZbufferStorageMethodEnum::ZBUF_24)
    {
    }
    
    ZBuffer::ZBuffer(unsigned int width, unsigned int height, unsigned int mask, bool enable, ZbufferTestMethod method, GSZbufferStorageMethodEnum zbufferStorageMethod)
        : GSbuffer(width, height, mask)
        , enable(enable)
        , method(method)
        , zbufferStorageMethod(zbufferStorageMethod)
    {
    }
    
     int ZBuffer::AllocateVRAMForBuffer()
    {
        address = graph_vram_allocate(this->width, this->height, static_cast<int>(this->zbufferStorageMethod), GRAPH_ALIGN_PAGE);
        return address;
    }
    
    void ZBuffer::ToSDKZbuffer(zbuffer_t* zb)
    {
        zb->enable = this->enable;
        zb->method = static_cast<int>(this->method),
        zb->address = address,
        zb->zsm = static_cast<int>(this->zbufferStorageMethod),
        zb->mask = mask;
    }
    
    u64 ZBuffer::GetZbufferSettings() const
    {
        return (u64(mask & 0x01) << 32 | u64(static_cast<u64>(zbufferStorageMethod) & 0xF) << 24 | u64(address >> 11 & 0x1FF));
    }
    
    ZbufferTestMethod ZBuffer::GetDepthTestMethod() const
    {
        return method;
    }
}
