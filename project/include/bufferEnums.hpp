
#pragma once

namespace Buffers{
    /*
    Used to specify how color data is stored inside a framebuffer, zbuffer or texture buffer
    */
    enum class GSPixelStorageMethod : unsigned int
    {
        PSM_32 = 0x00,
        /** 24 bits per pixel. */
        PSM_24 = 0x01,
        /** 16 bits per pixel. */
        PSM_16 = 0x02,
        /** 16 bits per pixel. */
        PSM_16S = 0x0A,
        /** 24 bits per pixel. */
        PSM_PS24 = 0x12,
        /** 8 bits per pix,el, palettized. */
        PSM_8 = 0x13,
        /** 4 bits per pixel, palettized. */
        PSM_4 = 0x14,
        /** 8 bits per pixel, 24 to 32 */
        PSM_8H = 0x1B,
        /** 4 bits per pixel, 28 to 32 */
        PSM_4HL = 0x24,
        /** 4 bits per pixel, 24 to 27 */
        PSM_4HH = 0x2C,
        /** 32 bits per pixel. */
        PSMZ_32 = 0x30,
        /** 24 bits per pixel. */
        PSMZ_24 = 0x31,
        /** 16 bits per pixel. */
        PSMZ_16 = 0x32,
        /** 16 bits per pixel. */
        PSMZ_16S = 0x3A
    };

    enum class GSZbufferStorageMethodEnum : unsigned int
    {
        /* 32 bit zbuffer */
        ZBUF_32 = 0x00,
        /* 24 bit zbuffer */
        ZBUF_24 = 0x01,
        /* 16 bit zbuffer */
        ZBUF_16 = 0x02,
        /* 12/24 bit compatible 16 bit zbuffer */
        ZBUF_16S = 0x0A
    };

    enum class ZbufferTestMethod : unsigned int
    {
        ALLFAIL = 0,
        ALLPASS = 1,
        GREATER_EQUAL = 2,
        GREATER = 3
    };

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
    enum class TextureWrappingOptions : unsigned char
    {
        REPEAT = 0,
        CLAMP = 1,
        REGION_CLAMP = 2,
        REGION_REPEAT = 3,
    };
    
}