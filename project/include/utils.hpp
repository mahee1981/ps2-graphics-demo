#pragma once
#include <tamtypes.h>


namespace Utils {
    static inline u16 FloatToFixedPoint(float x){
        constexpr unsigned int numberOfDecimalBits = 4;
        return u16(x * (1 << numberOfDecimalBits));
    }
}