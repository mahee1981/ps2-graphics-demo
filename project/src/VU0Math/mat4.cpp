#include <VU0Math/mat4.hpp>

ps2math::Mat4::Mat4(){
    std::fill(std::begin(data), std::end(data), 0);
    data[0] = 1.0f; // m11
    data[5] = 1.0f; // m22
    data[10] = 1.0f; // m33
    data[15] = 1.0f; // m44
}

ps2math::Mat4::Mat4(const float m11, const float m12, const float m13, const float m14,
                    const float m21, const float m22, const float m23, const float m24,
                    const float m31, const float m32, const float m33, const float m34,
                    const float m41, const float m42, const float m43, const float m44)
{
    data[0] = m11;
    data[1] = m12;
    data[2] = m13;
    data[3] = m14;

    data[4] = m21;
    data[5] = m22;
    data[6] = m23;
    data[7] = m24;

    data[8] = m31;
    data[9] = m32;
    data[10] = m33;
    data[11] = m34;

    data[12] = m41;
    data[13] = m42;
    data[14] = m43;
    data[15] = m44;
}

ps2math::Mat4::~Mat4(){

}

ps2math::Mat4 &ps2math::Mat4::RotateX(float angle)
{
    this->data[5] = cosf(angle);
    this->data[10] = cosf(angle);
    this->data[6] = -sinf(angle);
    this->data[9] = sinf(angle);

    return *this;
}

ps2math::Mat4 &ps2math::Mat4::RotateY(float angle)
{
    this->data[0] = cosf(angle);
    this->data[2] = sinf(angle);
    this->data[8] = -sinf(angle);
    this->data[10] = cosf(angle);

    return *this;
}

ps2math::Mat4 &ps2math::Mat4::RotateZ(float angle)
{
    this->data[0] = cosf(angle);
    this->data[1] = -sinf(angle);
    this->data[4] = sinf(angle);
    this->data[5] = cosf(angle);

    return *this;
}

ps2math::Mat4 ps2math::Mat4::identity()
{
    return Mat4();
}

ps2math::Vec4 ps2math::operator*(const Mat4 &lhs, const Vec4 &rhs)
{
    Vec4 work;
  asm volatile(
      "lqc2		$vf1, 0x00(%2)	\n"
      "lqc2		$vf2, 0x10(%2)	\n"
      "lqc2		$vf3, 0x20(%2)	\n"
      "lqc2		$vf4, 0x30(%2)	\n"
      "lqc2		$vf5, 0x00(%1)	\n"
      "vmulaw	$ACC, $vf4, $vf0\n"  //multiply last row with 1 of (0,0,0,1) and store it in ACC, giving the last row of the matrix
      "vmaddax	$ACC, $vf1, $vf5\n"  //
      "vmadday	$ACC, $vf2, $vf5\n"
      "vmaddz	$vf6, $vf3, $vf5\n"
      "sqc2		$vf6, 0x00(%0)	\n"
      :
      : "r"(&work), "r"(&rhs), "r"(lhs.GetDataPtr()));

  return work;

}
