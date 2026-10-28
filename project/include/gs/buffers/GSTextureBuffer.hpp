#ifndef GS_TEXTURE_BUFFER
#define GS_TEXTURE_BUFFER

#include "gs/buffers/GSBufferConfig.hpp"
#include "gs/buffers/GSbuffer.hpp"

namespace GS::Buffers
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
