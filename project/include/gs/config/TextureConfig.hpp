#ifndef TEXTURE_CONFIG_HPP
#define TEXTURE_CONFIG_HPP

namespace GS::Config
{
enum class TextureWrappingOptions : unsigned char
{
    REPEAT = 0,
    CLAMP = 1,
    REGION_CLAMP = 2,
    REGION_REPEAT = 3,
};
}

#endif
