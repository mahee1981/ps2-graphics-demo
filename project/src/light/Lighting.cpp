#include "light/Lighting.hpp"
#include <algorithm>

namespace Light
{

ps2math::Vec4 CalculateLighting(const ps2math::Vec4 &normal, const BaseLight &light)
{
    ps2math::Vec4 result;
    const ps2math::Vec4 &lightDir = light.GetDirection();
    const ps2math::Vec4 &lightColor = light.GetColor();
    float ambientIntensity = light.GetAmbientIntensity();
    float diffuseIntensity = light.GetDiffuseIntensity();

    // Calculate dot product using VU0 macro mode
    float dotProduct;
    asm volatile("lqc2 $vf1, 0x00(%1)    \n"
                 "lqc2 $vf2, 0x00(%2)    \n"
                 "vmul $vf3, $vf1, $vf2  \n"
                 "sqc2 $vf3, 0x00(%0)    \n"
                 :
                 : "r"(&result), "r"(&normal), "r"(&lightDir)
                 : "memory");

    dotProduct = -(result.x + result.y + result.z);
    dotProduct = std::max(0.0f, dotProduct);

    // Ambient + Diffuse
    float finalIntensity = ambientIntensity + (diffuseIntensity * dotProduct);
    finalIntensity = std::min(1.0f, finalIntensity);

    result.x = lightColor.x * finalIntensity;
    result.y = lightColor.y * finalIntensity;
    result.z = lightColor.z * finalIntensity;
    result.w = 1.0f;

    return result;
}

Colors::Color CalculateLightingRGBA8(const ps2math::Vec4 &normal, const BaseLight &light)
{
    ps2math::Vec4 color = 128.0f * CalculateLighting(normal, light);
    return Colors::Color(static_cast<u8>(color.x), static_cast<u8>(color.y), static_cast<u8>(color.z), static_cast<u8>(color.w));
}


} // namespace Light
