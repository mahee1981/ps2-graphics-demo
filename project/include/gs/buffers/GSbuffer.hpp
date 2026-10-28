#ifndef GSBUFFER_HPP
#define GSBUFFER_HPP

#include <graph_vram.h>

namespace GS::Buffers
{
class GSbuffer
{
  public:
    explicit GSbuffer();
    explicit GSbuffer(unsigned int width, unsigned int height, unsigned int mask);
    const unsigned int GetWidth() const;
    const unsigned int GetHeight() const;
    const unsigned int GetAddress() const;
    virtual ~GSbuffer();
    virtual int AllocateVRAMForBuffer() = 0;

  protected:
    const int width;
    const int height;
    const int mask;
    unsigned int address;
};
} // namespace GS::Buffers
#endif
