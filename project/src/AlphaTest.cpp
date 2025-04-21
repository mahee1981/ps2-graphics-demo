#include "AlphaTest.hpp"

AlphaTest::AlphaTest(bool isEnabled, AlphaTestMethod testMethod, u8 referenceAlphaValue,  AlphaTestOnFail onFail)
 : isEnabled(isEnabled), testMethod(testMethod), referenceAlphaValue(referenceAlphaValue), onFail(onFail)
{
    
}

u64 AlphaTest::GetAlphaTestSettings() const
{
    return (static_cast<u64>(onFail) & 0x03) << 12 | u64(referenceAlphaValue & 0xF) << 4 | (static_cast<u64>(testMethod) & 0x07) << 1 | u64(isEnabled & 0x01);
}
