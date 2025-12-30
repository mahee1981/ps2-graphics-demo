#ifndef UTILS_HPP
#define UTILS_HPP

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

const float pi = atan(1) * 4.0f;

float ToRadians(float angleDegrees);

} // namespace Utils
#endif
