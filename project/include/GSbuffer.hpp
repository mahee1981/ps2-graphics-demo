#pragma once

#include <graph_vram.h>
#include <IDisposable.hpp>
#include <stdio.h>

namespace Buffers
{
    class GSbuffer : public IDisposable
    {
    public:
        explicit GSbuffer();
        explicit GSbuffer(unsigned int width, unsigned int height, unsigned int mask);
        const unsigned int GetWidth() const;
        const unsigned int GetHeight() const;
        const unsigned int GetAddress() const;
        virtual ~GSbuffer();
        void Dispose() override;

    protected:
        const int width;
        const int height;
        const int mask;
        unsigned int address;
        virtual unsigned int AllocateVRAMForBuffer() = 0;
    };
}