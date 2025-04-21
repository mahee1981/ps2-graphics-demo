#include "graphics/GSTextureBuffer.hpp"

namespace Buffers
{

    GSTextureBuffer::GSTextureBuffer(unsigned int width, unsigned int height, GSPixelStorageMethod gsPSM)
        : GSbuffer(width, height, 0)
        , pixelStorageMethod(gsPSM)
    {
    }

    int GSTextureBuffer::AllocateVRAMForBuffer()
    {
        address = graph_vram_allocate(this->GetWidth(), this->GetHeight(), pixelStorageMethod, GRAPH_ALIGN_BLOCK);
        return address;
    }

    const int GSTextureBuffer::GetVramAddress() const
    {
        return address;
    }

    GSTextureBuffer::~GSTextureBuffer()
    {
    }
}
