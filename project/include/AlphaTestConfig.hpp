#ifndef ALPHA_TEST_CONFIG_HPP
#define ALPHA_TEST_CONFIG_HPP

enum class AlphaTestMethod : unsigned char
{
    NEVER = 0,
    ALWAYS = 1,
    LESS = 2,
    LESS_OR_EQUAL = 3,
    EQUAL = 4,
    GREATER_OR_EQUAL = 5,
    GREATER = 6,
    NOT_EQUAL = 7
};
enum class AlphaTestOnFail : unsigned char
{
    KEEP = 0,
    FB_UPDATE_ONLY = 1,
    ZB_UPDATE_ONLY = 2,
    RGB_UPDATE_ONLY = 3
};
#endif