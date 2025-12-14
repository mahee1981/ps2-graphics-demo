#ifndef TRANSFORM_HPP
#define TRANSFORM_HPP

#include "VU0Math/vec4.hpp"
#include <ostream>
namespace Components
{
class Transform

{
  public:
    Transform(const ps2math::Vec4 &translate,
              float angleX = 0.0f,
              float angleY = 0.0f,
              float angleZ = 0.0f,
              float scale = 1.0f);
    Transform();
    void SetTranslate(float x, float y, float z)
    {
        _position = ps2math::Vec4(x, y, z, 1.0f);
    }
    void TranslateBy(float x, float y, float z)
    {
        _position += ps2math::Vec4(x, y, z, 0.0f);
    }
    void SetAngleX(float angle)
    {
        _angleX = SetAngle(angle);
    }
    void RotateXBy(float angle)
    {
        _angleX = SetAngle(_angleX + angle);
    }
    void SetAngleY(float angle)
    {
        _angleY = SetAngle(angle);
    }
    void RotateYBy(float angle)
    {
        _angleY = SetAngle(_angleY + angle);
    }
    void SetAngleZ(float angle)
    {
        _angleZ = SetAngle(angle);
    }
    void RotateZBy(float angle)
    {
        _angleZ = SetAngle(_angleZ + angle);
    }
    void SetScaleFactor(float scaleFactor)
    {
        _scaleFactor = scaleFactor;
    }
    inline const ps2math::Vec4 &GetTranslate() const
    {
        return _position;
    };
    inline float GetAngleX() const
    {
        return _angleX;
    };
    inline float GetAngleY() const
    {
        return _angleY;
    };
    inline float GetAngleZ() const
    {
        return _angleZ;
    };
    inline float GetScaleFactor() const
    {
        return _scaleFactor;
    }
    friend std::ostream &operator<<(std::ostream &out, const Components::Transform &t);

  private:
    static float SetAngle(float angle);
    ps2math::Vec4 _position;
    float _angleX;
    float _angleY;
    float _angleZ;
    float _scaleFactor;
};
} // namespace Components

#endif
