#ifndef BLENDING_CONFIG_HPP
#define BLENDING_CONFIG_HPP

namespace GS::Config
{
enum class AlphaBlendingColorConfig : unsigned char
{
    COLOR_SOURCE = 0,
    COLOR_DESTINATION = 1,
    COLOR_ZERO = 2
};

enum class AlphaBlendingAlphaConfig : unsigned char
{
    ALPHA_SOURCE = 0,
    ALPHA_DESTINATION = 1,
    ALPHA_FIXED = 2
};
}
#endif
