#pragma once
#include <tamtypes.h>


namespace Utils {

    template<typename T>
    static inline T FloatToFixedPoint(float x){
        constexpr unsigned int numberOfDecimalBits = 4;
        return T(x * (1 << numberOfDecimalBits));
    }
    
}