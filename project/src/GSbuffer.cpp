
#include <GSbuffer.hpp>

namespace Buffers
{
    GSbuffer::GSbuffer() : width(640), height(480), mask(0), address(0)
    {
        
    }

    GSbuffer::GSbuffer(unsigned int width, unsigned int height, unsigned int mask) : width(width), height(height), mask(mask), address(0)
    {
    }

    GSbuffer::~GSbuffer()
    {
        graph_vram_free(address);
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
}