#pragma once
#include <tamtypes.h>


namespace Utils {
    static inline s16 FloatToFixedPoint(float x){
        constexpr unsigned int numberOfDecimalBits = 4;
        return s16(x * (1 << numberOfDecimalBits));
    }
}