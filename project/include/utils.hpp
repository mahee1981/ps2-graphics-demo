#ifndef UTILS_HPP
#define UTILS_HPP


#include <tamtypes.h>
#include <math.h>


namespace Utils {

    template<typename T>
    static inline T FloatToFixedPoint(float x){
        constexpr unsigned int numberOfDecimalBits = 4;
        return T(x * (1 << numberOfDecimalBits));
    }

    template <typename T>
        bool IsInBounds(const T& value, const T& low, const T& high) {
        return !(value < low) && !(high < value);

    }

    const float pi = atan(1) * 4.0f;

    float ToRadians(float angleDegrees);

    
}
#endif