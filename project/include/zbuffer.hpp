#pragma once

#include <GSbuffer.hpp>
#include <GSBufferConfig.hpp>
#include <draw_buffers.h>

namespace Buffers
{
    class ZBuffer : public GSbuffer
    {
    public:
        explicit ZBuffer();
        explicit ZBuffer(unsigned int width, unsigned int height, unsigned int mask, bool enable, ZbufferTestMethod method, GSZbufferStorageMethodEnum zbufferStorageMethod);
        void ToSDKZbuffer(zbuffer_t *);
        u64 GetZbufferSettings() const;
        Buffers::ZbufferTestMethod GetDepthTestMethod() const;

    private:
        bool enable;
        ZbufferTestMethod method;
        GSZbufferStorageMethodEnum zbufferStorageMethod;
        unsigned int AllocateVRAMForBuffer() override;
    };

}