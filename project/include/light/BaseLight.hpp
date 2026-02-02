#ifndef BASE_LIGHT_HPP
#define BASE_LIGHT_HPP

#include "VU0Math/vec4.hpp"
#include "packet2_types.h"

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
    packet2_t *packetInformation;
    void UpdatePacketInformation();

  public:
    BaseLight();
    ~BaseLight();
    inline void SetColor(const ps2math::Vec4 &newColor)
    {
        _color = newColor;
        UpdatePacketInformation();
    }
    inline void SetColor(float r, float g, float b)
    {
        _color = ps2math::Vec4(r, g, b, 1.0f);
        UpdatePacketInformation();
    }
    inline const ps2math::Vec4 &GetColor() const
    {
        return _color;
    }
    inline void SetAmbientIntensity(float newIntensity)
    {
        _ambientIntensity = newIntensity;
        UpdatePacketInformation();
    }
    inline const float GetAmbientIntensity() const
    {
        return _ambientIntensity;
    }
    inline void SetDiffuseIntensity(float newIntensity)
    {
        _diffuseIntensity = newIntensity;
        UpdatePacketInformation();
    }
    inline const float GetDiffuseIntensity() const
    {
        return _diffuseIntensity;
    }
    inline void SetSpecularIntensity(float newIntensity)
    {
        _specularIntensity = newIntensity;
        UpdatePacketInformation();
    }
    inline const float GetSpecularIntensity() const
    {
        return _specularIntensity;
    }

    inline void SetDirection(const ps2math::Vec4 &newDirection)
    {
        _direction = newDirection.Normalize();
        UpdatePacketInformation();
    }
    inline void SetDirection(float x, float y, float z)
    {
        _direction = ps2math::Vec4(x, y, z, 0.0f).Normalize();
        UpdatePacketInformation();
    }
    inline const ps2math::Vec4 &GetDirection() const
    {
        return _direction;
    }
    inline packet2_t *GetPacketInformation() const
    {
        return packetInformation;
    }
};
} // namespace Light

#endif
