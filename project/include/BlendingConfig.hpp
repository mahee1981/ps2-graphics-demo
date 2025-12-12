#ifndef BLENDING_CONFIG_HPP
#define BLENDING_CONFIG_HPP

enum AlphaBlendingColorConfig : unsigned char
{
    COLOR_SOURCE = 0,
    COLOR_DESTINATION = 1,
    COLOR_ZERO = 2
};

enum AlphaBlendingAlphaConfig : unsigned char
{
    ALPHA_SOURCE = 0,
    ALPHA_DESTINATION = 1,
    ALPHA_FIXED = 2
};
#endif
