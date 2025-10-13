#ifndef BASE_LIGHT_HPP
#define BASE_LIGHT_HPP

#include "VU0Math/vec4.hpp"

namespace Light
{
    class BaseLight
    {
    private:
        ps2math::Vec4 _color;
        float _ambientIntensity;
    public:
        BaseLight();
        inline void SetColor(const ps2math::Vec4 &newColor) { _color = newColor; }
        inline void SetColor(float r, float g, float b) { _color = ps2math::Vec4(r,g,b,1.0f); }
        inline const ps2math::Vec4 &GetColor() const { return _color; }
        inline void SetAmbientIntensity(float newIntensity) { _ambientIntensity = newIntensity; }
        inline const float GetAmbientIntensity() const { return _ambientIntensity; } 
    };
}

#endif