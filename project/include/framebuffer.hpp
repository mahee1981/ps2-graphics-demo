#pragma once

#include <draw_buffers.h>
#include <graph.h>

#include <GSbuffer.hpp>

#include <bufferEnums.hpp>

namespace Buffers
{

    class Framebuffer : public GSbuffer
    {
    public:
        explicit Framebuffer();
        explicit Framebuffer(unsigned int width, unsigned int height, unsigned int mask, GSPixelStorageMethod gsPSM);
        /* Sets the framebuffer to both drawing contexts*/
        void SetFramebufferAsActiveFilteredMode();
        /* Sets the framebuffer to a single drawing context*/
        void SetFramebufferAsActive(int context);

        void ToSDKFramebuffer(framebuffer_t *);

        u64 GetFrameBufferSettings();
        virtual ~Framebuffer();

    private:
        const GSPixelStorageMethod pixelStorageMethod;
        unsigned int AllocateVRAMForBuffer() override;
    };

}
