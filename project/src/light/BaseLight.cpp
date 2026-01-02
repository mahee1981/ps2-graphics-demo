#include "light/BaseLight.hpp"

namespace Light
{
BaseLight::BaseLight()
    : _color(1.0f, 1.0f, 1.0f, 1.0f), _direction(0.0f, -1.0f, 0.0f, 0.0f), _ambientIntensity(0.0f),
      _diffuseIntensity(0.0f), _specularIntensity(0.0f)
{
}

} // namespace Light
