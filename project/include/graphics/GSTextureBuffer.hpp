#ifndef GS_TEXTURE_BUFFER
#define GS_TEXTURE_BUFFER

#include "graphics/GSbuffer.hpp"
#include "graphics/GSBufferConfig.hpp"

namespace Buffers
{
    class GSTextureBuffer : public GSbuffer
    {
    public:
        explicit GSTextureBuffer(unsigned int width, unsigned int height, GSPixelStorageMethod gsPSM);

        int AllocateVRAMForBuffer() override;
        const int GetVramAddress() const;
        virtual ~GSTextureBuffer();

    private:
        const GSPixelStorageMethod pixelStorageMethod;
    };
}

#endif