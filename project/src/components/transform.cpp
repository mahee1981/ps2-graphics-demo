#include "components/transform.hpp"
#include <ostream>

namespace Components
{

Transform::Transform(const ps2math::Vec4 &translate, float angleX, float angleY, float angleZ, float scale)
    : _position(translate), _angleX(angleX), _angleY(angleY), _angleZ(angleZ), _scaleFactor(scale)
{
}
Transform::Transform() : Transform(ps2math::Vec4(0.0f, 0.0f, 0.0f, 1.0f))
{
}

float Transform::SetAngle(float angle)
{
    while (angle < 0.0f)
    {
        angle += 360.0f;
    }
    while (angle > 360.0f)
    {
        angle -= 360.0f;
    }
    return angle;
}
std::ostream &operator<<(std::ostream &out, const Components::Transform &t)
{
    out << "Position --> x: " << t.GetTranslate().x << ", y: " << t.GetTranslate().y << ", z: " << t.GetTranslate().y
        << "\n"
        << "Angle --> x: " << t.GetAngleX() << ", y: " << t.GetAngleY() << ", z: " << t.GetAngleZ() << "\n"
        << "Scale --> factor: " << t.GetScaleFactor();

    return out;
}
} // namespace Components
