#include <stdio.h>
#include "graphics/GSbuffer.hpp"

namespace Buffers
{
    GSbuffer::GSbuffer() : IDisposable(), width(640), height(480), mask(0), address(0)
    {
    }

    GSbuffer::GSbuffer(unsigned int width, unsigned int height, unsigned int mask)
        : IDisposable(), width(width), height(height), mask(mask), address(0)
    {
    }

    GSbuffer::~GSbuffer()
    {
        Dispose();
    }
    const unsigned int GSbuffer::GetHeight() const
    {
        return height;
    }

    const unsigned int GSbuffer::GetWidth() const
    {
        return width;
    }

    const unsigned int GSbuffer::GetAddress() const
    {
        return address;
    }
    void GSbuffer::Dispose()
    {
        if (!disposed)
        {
            printf("Freed Memory!\n");
            // sleep(2);
            if (address)
                graph_vram_free(address);
            disposed = true;
        }
    }
}
