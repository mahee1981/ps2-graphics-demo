#ifndef GS_TEXTURE_BUFFER
#define GS_TEXTURE_BUFFER

#include "graphics/GSBufferConfig.hpp"
#include "graphics/GSbuffer.hpp"

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
} // namespace Buffers

#endif
