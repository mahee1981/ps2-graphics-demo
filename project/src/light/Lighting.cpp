#include "light/Lighting.hpp"
#include <algorithm>

namespace Light
{

ps2math::Vec4 CalculateLighting(const ps2math::Vec4 &normal, const BaseLight &light)
{
    const ps2math::Vec4 &lightDir = light.GetDirection();
    const ps2math::Vec4 &lightColor = light.GetColor();
    float ambientIntensity = light.GetAmbientIntensity();
    float diffuseIntensity = light.GetDiffuseIntensity();
    float specularIntensity = light.GetSpecularIntensity();

    float diffuseImpact = -(ps2math::DotProduct(normal, lightDir));
    diffuseImpact = std::max(0.0f, diffuseImpact);



    // Ambient + Diffuse
    float finalIntensity = ambientIntensity + (diffuseIntensity * diffuseImpact);
    finalIntensity = std::min(1.0f, finalIntensity);

    return lightColor * finalIntensity;
}

Colors::Color CalculateLightingRGBA8(const ps2math::Vec4 &normal, const BaseLight &light)
{
    ps2math::Vec4 color = 128.0f * CalculateLighting(normal, light);
    return Colors::Color(static_cast<u8>(color.x), static_cast<u8>(color.y), static_cast<u8>(color.z), static_cast<u8>(color.w));
}


} // namespace Light
