#ifndef ZBUFFER_HP
#define ZBUFFER_HP

#include <draw_buffers.h>
#include "graphics/GSbuffer.hpp"
#include "graphics/GSBufferConfig.hpp"

namespace Buffers
{
    class ZBuffer : public GSbuffer
    {
    public:
        explicit ZBuffer();
        explicit ZBuffer(unsigned int width, unsigned int height, unsigned int mask, bool enable, ZbufferTestMethod method, GSZbufferStorageMethodEnum zbufferStorageMethod);
        void ToSDKZbuffer(zbuffer_t *);
        u64 GetBufferSettings() const;
        Buffers::ZbufferTestMethod GetDepthTestMethod() const;
        int AllocateVRAMForBuffer() override;

    private:
        bool enable;
        ZbufferTestMethod method;
        GSZbufferStorageMethodEnum zbufferStorageMethod;
    };

}
#endif
