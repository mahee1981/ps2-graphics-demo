#ifndef ALPHA_TEST_HPP
#define ALPHA_TEST_HPP

#include "AlphaTestConfig.hpp"
#include <tamtypes.h>

class AlphaTest
{

  public:
    AlphaTest(bool isEnabled, AlphaTestMethod, u8 refValue, AlphaTestOnFail);
    u64 GetAlphaTestSettings() const;

  private:
    bool isEnabled;
    AlphaTestMethod testMethod;
    u8 referenceAlphaValue;
    AlphaTestOnFail onFail;
};

#endif
