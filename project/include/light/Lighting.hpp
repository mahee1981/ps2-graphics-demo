#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include "VU0Math/vec4.hpp"
#include "light/BaseLight.hpp"

namespace Light
{

ps2math::Vec4 CalculateLighting(const ps2math::Vec4 &normal, const BaseLight &light);

} // namespace Light

#endif
