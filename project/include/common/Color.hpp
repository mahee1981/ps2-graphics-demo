#ifndef COLOR_HPP
#define COLOR_HPP

#include "tamtypes.h"

namespace Colors
{

// specified as an unsigned int 0-255 or floating point value 0.0f-1.0f
class Color
{

  public:
    Color(u8 r, u8 g, u8 b, u8 a);
    Color();

    u8 r, g, b, a;

    inline float GetRedFloat() const { return float(r)/255.0f; }
    inline float GetBlueFloat() const { return float(b)/255.0f; }
    inline float GetGreenFloat() const { return float(g)/255.0f; }
    inline float GetAlphaFloat() const { return float(a)/255.0f; }

};
} // namespace Colors
#endif
