 #pragma once
 
enum class TextureWrappingOptions : unsigned char
{
    REPEAT = 0,
    CLAMP = 1,
    REGION_CLAMP = 2,
    REGION_REPEAT = 3,
};