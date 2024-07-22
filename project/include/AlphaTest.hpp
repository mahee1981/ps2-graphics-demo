#pragma once

#include <bufferEnums.hpp>
#include <tamtypes.h>

class AlphaTest{

public:
    AlphaTest(bool isEnabled, Buffers::AlphaTestMethod, u8 refValue, Buffers::AlphaTestOnFail);
    u64 GetAlphaTestSettings();

private:
    bool isEnabled;
    Buffers::AlphaTestMethod testMethod;
    u8 referenceAlphaValue;
    Buffers::AlphaTestOnFail onFail;
};