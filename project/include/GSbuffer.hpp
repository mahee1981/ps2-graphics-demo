#pragma once

#include <graph_vram.h>

namespace Buffers
{
    class GSbuffer
    {
    public:
        explicit GSbuffer();
        explicit GSbuffer(unsigned int width, unsigned int height, unsigned int mask);
        const unsigned int GetWidth();
        const unsigned int GetHeight();
        const unsigned int GetAddress();
        virtual ~GSbuffer();

    protected:
        const int width;
        const int height;
        const int mask;
        unsigned int address;
        virtual unsigned int AllocateVRAMForBuffer() = 0;
    };
}