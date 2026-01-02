#ifndef BASE_LIGHT_HPP
#define BASE_LIGHT_HPP

#include "VU0Math/vec4.hpp"

namespace Light
{
class BaseLight
{
  private:
    ps2math::Vec4 _color;
    ps2math::Vec4 _direction;
    float _ambientIntensity;
    float _diffuseIntensity;
    float _specularIntensity;

  public:
    BaseLight();
    inline void SetColor(const ps2math::Vec4 &newColor)
    {
        _color = newColor;
    }
    inline void SetColor(float r, float g, float b)
    {
        _color = ps2math::Vec4(r, g, b, 1.0f);
    }
    inline const ps2math::Vec4 &GetColor() const
    {
        return _color;
    }
    inline void SetAmbientIntensity(float newIntensity)
    {
        _ambientIntensity = newIntensity;
    }
    inline const float GetAmbientIntensity() const
    {
        return _ambientIntensity;
    }
    inline void SetDiffuseIntensity(float newIntensity)
    {
        _diffuseIntensity = newIntensity;
    }
    inline const float GetDiffuseIntensity() const
    {
        return _diffuseIntensity;
    }
    inline void SetSpecularIntensity(float newIntensity)
    {
        _specularIntensity = newIntensity;
    }
    inline const float GetSpecularIntensity() const
    {
        return _specularIntensity;
    }

    inline void SetDirection(const ps2math::Vec4 &newDirection)
    {
        _direction = newDirection.Normalize();
    }
    inline void SetDirection(float x, float y, float z)
    {
        _direction = ps2math::Vec4(x, y, z, 0.0f).Normalize();
    }
    inline const ps2math::Vec4 &GetDirection() const
    {
        return _direction;
    }
};
} // namespace Light

#endif
