#ifndef UTILS_HPP
#define UTILS_HPP

#include "draw_primitives.h"
#include <math.h>
#include <tamtypes.h>

namespace Utils
{

template <typename T, int NFracPart = 4> static inline T FloatToFixedPoint(float x)
{
    constexpr unsigned int numberOfDecimalBits = NFracPart;
    return T(x * (1 << numberOfDecimalBits));
}

template <typename T> bool IsInBounds(const T &value, const T &low, const T &high)
{
    return !(value < low) && !(high < value);
}

constexpr u64 GetPrimGSValue(prim_t prim)
{
    return (prim.colorfix << 10) | (0 << 9) | (prim.mapping_type << 8) | (prim.antialiasing << 7) | (prim.blending << 6) | (prim.fogging << 5) | (prim.mapping << 4) | (prim.shading << 3) | (prim.type);
}

const float pi = atan(1) * 4.0f;

float ToRadians(float angleDegrees);

} // namespace Utils
#endif
