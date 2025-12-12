#ifndef ZBUFFER_HP
#define ZBUFFER_HP

#include "graphics/GSBufferConfig.hpp"
#include "graphics/GSbuffer.hpp"
#include <draw_buffers.h>

namespace Buffers
{
class ZBuffer : public GSbuffer
{
  public:
    explicit ZBuffer();
    explicit ZBuffer(unsigned int width,
                     unsigned int height,
                     unsigned int mask,
                     bool enable,
                     ZbufferTestMethod method,
                     GSZbufferStorageMethodEnum zbufferStorageMethod);
    void ToSDKZbuffer(zbuffer_t *);
    u64 GetBufferSettings() const;
    Buffers::ZbufferTestMethod GetDepthTestMethod() const;
    int AllocateVRAMForBuffer() override;

  private:
    bool enable;
    ZbufferTestMethod method;
    GSZbufferStorageMethodEnum zbufferStorageMethod;
};

} // namespace Buffers
#endif
